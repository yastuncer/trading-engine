# Order Book Forecasting on a Custom Matching Engine

This is a project I'm building to predict short-horizon mid-price moves on NASDAQ order book data, using a matching engine I wrote in C++ with Python bindings on top. The matching engine handles replay of historical data and runs the backtests; the Python side does feature engineering, model training, and evaluation.

This project is a research-style exercise meant to show I can do the full pipeline (real data, real evaluation, real backtest) with proper methodology. It isn't a trading strategy that prints money. 

Why build a matching engine when there are libraries like Backtrader for backtesting? Backtrader is good for strategy backtesting but doesn't simulate an actual order book. My simulated orders wouldn't compete for queue position against the historical orders the way they would on a real exchange. For this project I needed closed-loop simulation, where the predictions I generate get fed back into a book that behaves like a real one. Writing the engine myself also forced me to actually understand price-time priority, partial fills, and order modify semantics, which made the modeling work less mysterious.

---

## Table of Contents

1. [Problem](#1-problem)
2. [System architecture](#2-system-architecture)
3. [Data pipeline](#3-data-pipeline)
4. [Modeling approach](#4-modeling-approach)
5. [Evaluation methodology](#5-evaluation-methodology)
6. [Results](#6-results)
7. [Build and reproduce](#7-build-and-reproduce)
8. [What I learned](#8-what-i-learned)
9. [What's next](#9-whats-next)
10. [References](#10-references)

---

## 1. Problem

Modern equity markets execute trades through electronic limit order books. At any moment, the book lists every resting order with its price, quantity, side, and arrival time, and it gets updated thousands of times per second. There's information in that state about where the price is about to move, but most of it doesn't last long. By the time you've reacted to a signal, the conditions that created it may have shifted.

The question I'm trying to answer is whether a machine learning model, given the current state of the order book and recent event history, can predict the direction of the mid-price over the next K events more reliably than well-known microstructure baselines (sign of order flow imbalance, microprice drift, simple persistence).

This is a problem I picked because it tests a full applied-ML pipeline: time-ordered data, imbalanced classes, leakage risk, and strong baselines. The interesting work isn't producing a number, it's evaluating models rigorously enough to know which approach actually wins.

### Background (for readers new to market microstructure)

A limit order book is the data structure an electronic exchange uses to match buyers and sellers. It lists every resting order: the price someone is willing to trade at, how many units they want, which side they're on, and when they arrived. The highest buy price is the best bid, the lowest sell price is the best ask, the gap between them is the spread, and the average of the two is the mid-price.

Orders match by price-time priority. Better prices match first, and at the same price, the order that arrived first gets filled first. When a marketable order comes in, it executes against resting orders on the opposite side until it's either fully filled or there's no more matching liquidity at acceptable prices.

The book carries short-lived information about where the price is heading. If there's a lot more volume resting on the bid side than the ask side, the next price move is slightly more likely to be downward, because sellers have to chew through more demand than buyers do. Picking up on this kind of asymmetry across many features is the goal.

### Related work

A few papers that motivate the approach. Cont, Kukanov and Stoikov (2014) show that a simple linear function of order flow imbalance explains a surprising fraction of short-horizon mid-price changes, which makes OFI a hard baseline for any ML model to clear. Kercheval and Zhang (2015) introduced ML-based forecasting on LOB features, and Zhang, Zohren and Roberts (2019), known as DeepLOB, showed that a CNN-LSTM applied to raw normalized book states can outperform hand-engineered features at short horizons. Avellaneda and Stoikov (2008) derive a closed-form optimal market-making policy that I'll use as a benchmark if I get to the planned v2 extension.

---

## 2. System architecture

```
┌──────────────────┐
│ LOBSTER messages │   raw event-by-event NASDAQ data
│   (CSV input)    │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Message parser  │   LOBSTER msg type → engine command
│     (Python)     │
└────────┬─────────┘
         │
         ├──────────────────────┐
         ▼                      ▼
┌──────────────────┐   ┌──────────────────┐
│   C++ engine     │   │ Feature builder  │
│   (pybind11)     │   │     (NumPy)      │
│                  │   │                  │
│ • Order book     │   │ • OFI windows    │
│ • Matching       │   │ • Microprice     │
│ • Snapshots      │   │ • Imbalance L1–5 │
└────────┬─────────┘   │ • Queue depth    │
         │             │ • Trade flow     │
         │             │ • Vol estimators │
         │             └────────┬─────────┘
         │                      │
         │                      ▼
         │             ┌──────────────────┐
         │             │     Models       │
         │             │                  │
         │             │ • Baselines      │
         │             │ • LightGBM       │
         │             │ • CNN-LSTM       │
         │             └────────┬─────────┘
         │                      │
         └──────────┬───────────┘
                    ▼
         ┌──────────────────────┐
         │  Closed-loop backtest │   model predictions → orders → 
         │   (engine replay)     │   matched against historical book
         └──────────┬────────────┘
                    │
                    ▼
         ┌──────────────────────┐
         │ Evaluation + reports │   walk-forward metrics, ablations,
         │   (MLflow tracked)    │   PnL curves, calibration plots
         └──────────────────────┘
```

A few notes on why the architecture looks like this. The engine is in C++ because matching has to be deterministic and reasonably fast; everything else is Python because iteration speed matters more than runtime during research. The bindings use pybind11, which is header-only and works well with NumPy. I'm using the same engine for both training feature generation and backtesting, which avoids a common pitfall where features computed during training don't match features computed at inference; if there's an engine bug, both paths see it and there's no place for a leakage problem to hide. Experiment tracking goes through MLflow from the start, so I don't end up with a mess of files named after the date and a vague suffix.

---

## 3. Data pipeline

### Source

I'm using the academic sample from LOBSTER, which provides message-level (Level 3) order book data derived from NASDAQ ITCH feeds for a small set of tickers (AAPL, AMZN, GOOG, INTC, MSFT, SPY are the usual ones). Each session comes as two CSVs:

- `*_message_*.csv`: one row per event (submission, cancel, execution, etc.)
- `*_orderbook_*.csv`: snapshot of the top N levels of the book after each event

LOBSTER message types and how my engine handles them:

| Type | Meaning                          | Engine action          |
|------|----------------------------------|------------------------|
| 1    | New limit order submission       | `add(order)`           |
| 2    | Cancellation (partial)           | `modify(id, new_qty)`  |
| 3    | Cancellation (full deletion)     | `cancel(id)`           |
| 4    | Execution (visible)              | `process(market_order)` |
| 5    | Execution (hidden)               | trade event only       |
| 7    | Trading halt                     | end of session         |

### Engine and LOBSTER reconciliation

This is the correctness check that everything else depends on. After replaying each message into my engine, I compare the resulting top-of-book against LOBSTER's snapshot file for that timestamp. If they diverge, there's a bug somewhere in the engine, usually in how I'm handling hidden executions or in modify semantics for partial cancels. The replay script prints the first divergence at message granularity so I can see exactly where things go wrong.

Hidden orders (type 5) are a tricky case. NASDAQ allows some order types that don't display in the visible book, used mostly by institutional traders to avoid signaling their intent. LOBSTER sees them when they execute but never while they're resting, so my engine treats a type-5 execution as a trade event for feature purposes (the volume is real and matters) but doesn't try to model the hidden order itself.

### Feature pipeline

Features are computed during replay, on the book state immediately *before* each event, which prevents lookahead. The feature groups:

- Top-of-book: spread $s_t = P^A_t - P^B_t$, mid $m_t = (P^A_t + P^B_t)/2$, and microprice

$$
\tilde{m}_t = \frac{q^A_t \cdot P^B_t + q^B_t \cdot P^A_t}{q^A_t + q^B_t}
$$

The microprice weights the mid toward whichever side has less resting volume; it's approximately a martingale under standard assumptions (Stoikov 2018), which makes it a better short-horizon estimator than the simple average.

- Imbalance: bid/ask volume imbalance at depths 1, 3, 5
- Order flow imbalance (OFI) from Cont, Kukanov & Stoikov (2014). For each event $n$ at the top of book, define the event contribution

$$
e_n = \mathbb{1}_{P_n^B \geq P_{n-1}^B} \cdot q_n^B - \mathbb{1}_{P_n^B \leq P_{n-1}^B} \cdot q_{n-1}^B - \mathbb{1}_{P_n^A \leq P_{n-1}^A} \cdot q_n^A + \mathbb{1}_{P_n^A \geq P_{n-1}^A} \cdot q_{n-1}^A
$$

where $P^B, q^B$ are best-bid price and size and $P^A, q^A$ are best-ask price and size. OFI over a window is $\text{OFI}_W = \sum_{n \in W} e_n$. Computed over windows of 10, 50, and 200 events.

- Queue depth: cumulative resting volume at the top N levels
- Trade flow: signed trade volume over the last K events
- Realized volatility: rolling standard deviation of mid-price returns over a short window

Everything is normalized per-session to handle regime differences across days and tickers.

### Labels

For each event, the label is the direction of mid-price K events into the future, with a no-move band:

```
label = up    if mid[t+K] - mid[t] > +epsilon
        down  if mid[t+K] - mid[t] < -epsilon
        flat  otherwise
```

`epsilon` is set to one tick. K gets evaluated at 10, 50, and 100 events.

---

## 4. Modeling approach

Three tiers of models, in order of complexity. Each tier has to outperform the previous one to justify being there.

### Tier 1: Baselines

I don't think of these as throwaway models; they're the bar. The baselines I'm running:

- Majority class: predicts the most common label in train (almost always `flat` at short K)
- Persistence: predicts the sign of the previous mid-price move
- `sign(OFI)`: predicts up/down by the sign of order flow imbalance over a tuned window
- Logistic regression on a hand-picked set of six features

### Tier 2: Gradient-boosted trees (LightGBM)

Multi-class classification on the full feature set, tuned with Bayesian search over learning rate, num leaves, min data in leaf, and the feature and bagging fractions. Class weights are set inverse to frequency to handle the imbalance toward `flat`. LightGBM is more or less the right baseline ML model for tabular microstructure work; it handles non-linear interactions, trains fast, and gives you feature importance for free.

### Tier 3: Sequence model (1D-CNN + LSTM)

Inspired by DeepLOB (Zhang, Zohren and Roberts 2019). The input is a tensor of shape `(time_window, 4 * depth)`, which is the last T events' worth of normalized `(bid_price, bid_qty, ask_price, ask_qty)` at the top L levels.

```
Input → [Conv1D → Conv1D → MaxPool] × 2 → LSTM(64) → Dense(3) → Softmax
```

Trained with cross-entropy loss and AdamW, with early stopping on validation log-loss. PyTorch.

---

## 5. Evaluation methodology

### Two principles I'm being deliberate about

**No lookahead.** Features are computed only from events at or before the prediction point. Normalization statistics come from training data only and are applied unchanged to validation and test. This is the most common way microstructure ML projects accidentally cheat.

**Baselines.** OFI is a published, well-tested predictor. The goal of this project is to evaluate honestly where ML models add value and where they don't, with a comparison setup that makes the answer interpretable either way.


### Time-ordered splits

Splits are chronological:

- Train: first 60% of sessions
- Validation: next 20%
- Test: final 20%

The test set gets touched once, at the end, after all model choices are frozen.

### Walk-forward (rolling-origin) evaluation

For the headline results, the test split is broken into windows and the model is retrained on an expanding window of training data, then evaluated on the next held-out window. This gives me a distribution of test metrics across multiple time periods rather than a single point estimate, which is much more informative.

### Metrics

Reported with bootstrap confidence intervals across walk-forward windows.

| Metric             | Why included                                  |
|--------------------|------------------------------------------------|
| Accuracy           | Familiar, but misleading with class imbalance |
| Balanced accuracy  | Accounts for `flat` dominance                  |
| F1 (per class)     | Direction-of-move quality, not just `flat`     |
| Log-loss           | Calibration-aware                              |
| ROC-AUC (up-vs-rest, down-vs-rest) | Threshold-free signal quality |

### Ablations

For the best model, I drop each feature group and retrain, then report the change in balanced accuracy. This shows which features are actually carrying information versus which the model could happily live without.

### Calibration

Reliability diagrams per class. If the model says "70% up," it should be right 70% of the time when it makes that claim. Poor calibration matters because any downstream use that involves a threshold will misbehave without it.

### Closed-loop backtest

The model's predictions drive a simple market-making strategy through the engine. When the model predicts `up` with confidence above a threshold, it posts a buy at the bid; symmetric for `down`; cancel and re-quote on each event; realistic costs applied (half-spread plus a per-share fee).

The whole thing runs against the historical LOBSTER replay, with simulated orders queueing against the real historical book. I compare PnL against the same strategy driven by `sign(OFI)`.

Comparing classification accuracy to backtest PnL is informative either way — the models might rank the same on both, or they might not, and either result tells me something about which approach is actually useful.

---

## 6. Results

> **TODO (end of project):** Fill in this section with final numbers, confusion matrices, learning curves, ablation tables, equity curves. Below is the structure I'll fill in.

### Headline metrics

| Model              | Bal. Acc. (K=50) | F1-up | F1-down | Log-loss |
|--------------------|------------------|-------|---------|----------|
| Majority class     | TODO             | TODO  | TODO    | TODO     |
| Persistence        | TODO             | TODO  | TODO    | TODO     |
| sign(OFI)          | TODO             | TODO  | TODO    | TODO     |
| Logistic regression| TODO             | TODO  | TODO    | TODO     |
| LightGBM           | TODO             | TODO  | TODO    | TODO     |
| CNN-LSTM           | TODO             | TODO  | TODO    | TODO     |

### Performance vs horizon

> TODO: insert plot of balanced accuracy vs K ∈ {10, 50, 100}.

### Feature ablation

> TODO: insert ablation table — change in balanced accuracy when each feature group is removed.

### Calibration

> TODO: insert reliability diagrams per class.

### Backtest

> TODO: insert equity curves for LightGBM strategy vs `sign(OFI)` strategy on out-of-sample sessions, with cost assumptions stated explicitly.

---

## 7. Build and reproduce

```bash
# 1. Clone
git clone https://github.com/USER/lob-forecasting
cd lob-forecasting

# 2. Build C++ engine (requires C++17 compiler, CMake ≥ 3.18, pybind11)
cmake -B build -S engine
cmake --build build -j

# 3. Python env
python -m venv .venv && source .venv/bin/activate
pip install -e .
pip install -r requirements.txt

# 4. Get LOBSTER sample data
python scripts/download_lobster_sample.py --out data/raw

# 5. Replay and validate engine against LOBSTER snapshots
python scripts/replay.py --ticker AAPL --validate

# 6. Build features and labels
python scripts/build_dataset.py --tickers AAPL,MSFT,SPY --horizons 10,50,100

# 7. Train all models
python scripts/train.py --config configs/all.yaml

# 8. Run walk-forward evaluation + backtest
python scripts/evaluate.py --config configs/walkforward.yaml
```

All artifacts (models, metrics, plots) get written to `mlruns/`. The MLflow UI is available via `mlflow ui`.

---

## 8. What I learned

> **TODO:** I'll fill this in honestly at the end of the project. Things I'm planning to write about:

- Engine correctness: what bugs surfaced via the LOBSTER snapshot reconciliation? What did the first divergence look like?
- Feature engineering: which features ended up mattering? Were there features I expected to matter that didn't?
- Modeling: did the sequence model beat LightGBM by enough to justify the extra complexity? What was the train/inference cost trade-off?
- Evaluation: where did I almost introduce leakage? What checks caught it?
- Backtest vs classification: did the rank order of models change between offline metrics and the closed-loop test? Why?

---

## 9. What's next

The planned follow-up after this project ships is a reinforcement-learning market-maker built on the same engine. The supervised model in this project predicts price direction; the v2 agent would quote both sides of the book and learn a market-making policy with PPO, evaluated against the closed-form Avellaneda-Stoikov optimal market maker (2008) and against the supervised policy from this project, converted into a quote-skew rule. The engine, features, and evaluation harness are designed so that extension is relatively cheap to add.

A few other extensions I might do, roughly in order of effort:

1. More tickers, more sessions. With paid LOBSTER data or NASDAQ ITCH samples, retrain across regimes (high-vol vs low-vol days, different sectors).
2. Transformer architectures. Try a temporal fusion transformer on the same feature representation.
3. Multi-horizon and multi-task heads. Predict K=10, 50, 100 jointly with a shared encoder.
4. Latency-realistic backtest. The closed-loop test currently assumes zero-latency reactions; adding a configurable delay would show how the signal value decays.

---

## 10. References

- Avellaneda, M., Stoikov, S. (2008). *High-frequency trading in a limit order book.* Quantitative Finance, 8(3), 217–224.
- Cont, R., Kukanov, A., Stoikov, S. (2014). *The price impact of order book events.* Journal of Financial Econometrics, 12(1), 47–88.
- Gould, M. D., Porter, M. A., Williams, S., McDonald, M., Fenn, D. J., Howison, S. D. (2013). *Limit order books.* Quantitative Finance, 13(11), 1709–1742.
- Huang, R., Polak, T. (2011). *LOBSTER: Limit Order Book Reconstructor.* https://lobsterdata.com
- Kercheval, A. N., Zhang, Y. (2015). *Modelling high-frequency limit order book dynamics with support vector machines.* Quantitative Finance, 15(8), 1315–1329.
- Stoikov, S. (2018). *The micro-price: a high-frequency estimator of future prices.* Quantitative Finance, 18(12), 1959–1966.
- Zhang, Z., Zohren, S., Roberts, S. (2019). *DeepLOB: Deep convolutional neural networks for limit order books.* IEEE Transactions on Signal Processing, 67(11), 3001–3012.
