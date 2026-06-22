# Order Book Forecasting on a Custom Matching Engine

Short-horizon mid-price forecasting on NASDAQ LOBSTER data, built on a custom C++ limit order book engine with Python bindings. The model pipeline is evaluated with walk-forward validation and closed-loop backtesting through the same engine that produced the features — so training features and live features come from identical code.

> **What this is:** a research-quality microstructure forecasting pipeline with an end-to-end, reproducible evaluation.
> **What this isn't:** a profitable trading strategy. The point is rigorous methodology, not a backtest beating the market.

Why build a trading engine when there are existing platforms out there?

> Backtrader is great for strategy backtesting, but it doesn't simulate an actual order book, my simulated orders don't compete for queue position against historical orders. For this project I needed closed-loop simulation, where my model's orders interact with the historical book the same way they would on a real exchange. The custom engine gave me that. The secondary benefit was that writing it forced me to understand price-time priority, partial fills, and modify semantics from the inside, which made the modeling work better because I knew exactly what features the book actually contained. >

---

## Table of Contents

1. [Problem](#1-problem)
2. [System architecture](#2-system-architecture)
3. [Data pipeline](#3-data-pipeline)
4. [Modeling approach](#4-modeling-approach)
5. [Evaluation methodology](#5-evaluation-methodology)
6. [Results](#6-results)
7. [Build & reproduce](#7-build--reproduce)
8. [What I learned](#8-what-i-learned)
9. [What's next](#9-whats-next)
10. [References](#10-references)

---

## 1. Problem

Modern equity markets execute trades through electronic limit order books (LOBs). At any instant, the LOB summarises *every* resting order — price, quantity, side, and arrival time — and updates thousands of times per second. The book's state contains a great deal of information about near-term price moves, but most of it is conditional and short-lived: by the time you've reacted to a signal, the regime that produced it may have changed.

**The forecasting question.** Given the current state of the order book and recent event history, can a model predict the *direction* of the mid-price over the next K events more reliably than well-understood microstructure baselines (sign of order flow imbalance, microprice drift, persistence)?

**Why this problem.** Three reasons:

1. **It's a real applied-ML pipeline.** Time-ordered data, severe class imbalance, leakage risk everywhere, baselines that are genuinely hard to beat. Forces you to do evaluation correctly or get embarrassed by the test set.
2. **The baselines are strong.** Order flow imbalance (OFI), in particular, is a published, well-understood predictor. Beating it cleanly — or honestly reporting that you didn't — is more informative than chasing accuracy on a vanity task.
3. **The substrate is reusable.** Once the engine and feature pipeline exist, swapping in different models, horizons, or instruments is mechanical.

### Background (for readers new to market microstructure)

A **limit order book (LOB)** is the data structure an electronic exchange uses to match buyers and sellers. At any instant it lists every resting order: the price someone is willing to trade at, how many units they want, which side (buy/sell), and when the order arrived. The highest buy price is the **best bid**; the lowest sell price is the **best ask**; the gap between them is the **spread**; the average of the two is the **mid-price**.

Orders match by **price-time priority**: better prices match first, and at the same price, earlier orders match first. When a marketable order arrives, it executes against resting orders on the opposite side until either it's filled or no more matching liquidity exists.

The state of the book carries short-lived information about near-term price moves. For example, if there's much more volume resting on the bid side than the ask side, the next price move is *slightly* more likely to be downward — sellers have to chew through more demand than buyers do. Capturing this kind of asymmetry across many features is the goal here.

### Related work

Three lines of work motivate this project. **Cont, Kukanov & Stoikov (2014)** show that a simple linear function of *order flow imbalance* (OFI) explains a large fraction of short-horizon mid-price changes — a hard baseline for any ML model to beat. **Kercheval & Zhang (2015)** and follow-ups introduced ML-based forecasting on LOB features, with **Zhang, Zohren & Roberts (2019, "DeepLOB")** demonstrating that a CNN-LSTM on raw normalized book states can outperform hand-engineered features at short horizons. Separately, **Avellaneda & Stoikov (2008)** derive the closed-form optimal market-making policy that the v2 RL extension uses as a benchmark. This project sits in the first two lines; v2 extends into the third.

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

**Key design choices:**

- **C++ core, Python everything else.** The engine handles the work that needs to be deterministic and fast (book maintenance, matching). Everything else (parsing, features, models, plots) is Python because iteration speed matters more than latency during research.
- **Bindings via pybind11.** Header-only, integrates cleanly with CMake, zero-copy NumPy interop for snapshot extraction.
- **Same engine for training and backtest.** Avoids the common error where backtest features are computed differently from training features. If the engine has a bug, both pipelines see it; you can't accidentally hide a leakage problem in the eval path.
- **Experiment tracking from day one.** Every run logged with MLflow: config, code hash, metrics, artifacts. No `model_v3_final_FINAL.pkl`.

---

## 3. Data pipeline

### Source

NASDAQ LOBSTER academic sample — message-level ITCH-derived order book data for a small set of tickers (AAPL, AMZN, GOOG, INTC, MSFT, SPY are typical). Each session is delivered as two CSVs:

- `*_message_*.csv`: one row per event (submission, cancel, execution, etc.)
- `*_orderbook_*.csv`: snapshot of the top N levels of the book *after* each event

LOBSTER message types used:

| Type | Meaning                          | Engine action          |
|------|----------------------------------|------------------------|
| 1    | New limit order submission       | `add(order)`           |
| 2    | Cancellation (partial)           | `modify(id, new_qty)`  |
| 3    | Cancellation (full deletion)     | `cancel(id)`           |
| 4    | Execution (visible)              | `process(market_order)` |
| 5    | Execution (hidden)               | (handled separately — see below) |
| 7    | Trading halt                     | end of usable session  |

### Engine ↔ LOBSTER reconciliation

The reconciliation step is the single most important correctness check. After every event, the engine's top-of-book is compared against LOBSTER's snapshot file. If they diverge, the engine has a bug — usually in handling hidden executions or modify semantics. The replay script reports first divergence at message granularity, which makes debugging tractable.

**Hidden orders (type 5):** LOBSTER's visible book doesn't contain them, so the engine doesn't either. A type-5 execution is treated as a trade event for downstream features (it's still real volume) but doesn't touch book state.

### Feature pipeline

Features are computed on-the-fly during replay. Each event produces a feature vector aligned with the book snapshot *immediately before* the event — this prevents lookahead.

Feature groups:

- **Top-of-book**: spread $s_t = P^A_t - P^B_t$, mid $m_t = (P^A_t + P^B_t)/2$, and microprice

$$
\tilde{m}_t = \frac{q^A_t \cdot P^B_t + q^B_t \cdot P^A_t}{q^A_t + q^B_t}
$$

which weights the mid toward whichever side has less resting volume. The microprice is approximately a martingale under standard assumptions (Stoikov 2018), making it a better short-horizon mid estimator than the simple average.
- **Imbalance**: bid/ask volume imbalance at depths 1, 3, 5
- **Order flow imbalance (OFI)** from Cont, Kukanov & Stoikov (2014). For each event $n$ at the top of book, define the event contribution

$$
e_n = \mathbb{1}_{P_n^B \geq P_{n-1}^B} \cdot q_n^B - \mathbb{1}_{P_n^B \leq P_{n-1}^B} \cdot q_{n-1}^B - \mathbb{1}_{P_n^A \leq P_{n-1}^A} \cdot q_n^A + \mathbb{1}_{P_n^A \geq P_{n-1}^A} \cdot q_{n-1}^A
$$

where $P^B, q^B$ are best-bid price/size and $P^A, q^A$ are best-ask price/size. OFI over a window is $\text{OFI}_W = \sum_{n \in W} e_n$. Computed over rolling windows of 10, 50, and 200 events.
- **Queue depth**: cumulative volume at top N levels
- **Trade flow**: signed trade volume over the last K events
- **Realized volatility**: rolling stddev of mid-price returns over short windows

All features are normalized per-session to handle regime differences across days/tickers.

### Labels

For each event, the label is the direction of mid-price K events into the future, with a no-move band:

```
label = up    if mid[t+K] - mid[t] > +epsilon
        down  if mid[t+K] - mid[t] < -epsilon
        flat  otherwise
```

`epsilon` is set to one tick. K is evaluated at 10, 50, and 100 events.

---

## 4. Modeling approach

Three tiers, each tier must outperform the previous to justify the added complexity.

### Tier 1: Baselines

These are not throwaway models — they're the bar.

- **Majority class**: predicts the most common label in train (typically `flat` at short K)
- **Persistence**: predicts the sign of the previous mid-price move
- **`sign(OFI)`**: predicts up/down by the sign of order flow imbalance over a tuned window
- **Logistic regression**: on a hand-picked set of 6 features

### Tier 2: Gradient-boosted trees (LightGBM)

Multi-class classification on the full feature set. Tuned via Bayesian search over learning rate, num leaves, min data in leaf, and feature/bagging fractions. Class weights inverse to frequency to handle the imbalance toward `flat`.

LightGBM is the right baseline ML model for tabular microstructure work: handles non-linear interactions, fast to train, gives feature importance for free.

### Tier 3: Sequence model (1D-CNN + LSTM)

Inspired by DeepLOB (Zhang, Zohren, Roberts 2019). Input is a tensor of shape `(time_window, 4 * depth)` — the last T events' worth of normalized `(bid_price, bid_qty, ask_price, ask_qty)` across the top L levels.

Architecture:

```
Input → [Conv1D → Conv1D → MaxPool] × 2 → LSTM(64) → Dense(3) → Softmax
```

Trained with cross-entropy, AdamW, early stopping on validation log-loss. PyTorch.

---

## 5. Evaluation methodology

The thing that separates this project from a notebook.

### Time-ordered splits

**No shuffled splits anywhere.** Splits are chronological:

- Train: first 60% of sessions
- Validation: next 20%
- Test: final 20%

Touch the test set once, at the end, after all model choices are frozen.

### Walk-forward (rolling-origin) evaluation

For the headline results, the test split is broken into windows and the model is retrained on an expanding window of training data, then evaluated on the next held-out window. This produces a distribution of test metrics rather than a single point estimate.

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

For the best model, drop each feature group and re-train. Report Δ(balanced accuracy). Shows which features actually carry information vs. which the model could happily live without.

### Calibration

Reliability diagrams for each class. A model that says "70% up" should be right 70% of the time. Models that aren't calibrated are useless for any threshold-based downstream use.

### Closed-loop backtest

The model's predictions drive a simple market-making strategy through the engine:

- When the model predicts `up` with confidence > threshold, post a buy at bid
- Symmetric for `down`
- Cancel and re-quote on each event
- Apply realistic costs: half-spread + per-share fee

Run against the historical LOBSTER replay so simulated orders queue against the real historical book. Compare PnL against the same strategy driven by `sign(OFI)`.

**A model that beats baselines on classification accuracy but loses to them in backtest is a more honest finding than a model that wins both** — and reporting that clearly is the point of the exercise.

---

## 6. Results

> **TODO (end of week 4):** Fill in this section with final numbers, confusion matrices, learning curves, ablation tables, equity curves. Below is the structure.

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

> TODO: insert ablation table — Δ(bal. acc.) when each feature group is removed.

### Calibration

> TODO: insert reliability diagrams per class.

### Backtest

> TODO: insert equity curves for LightGBM strategy vs `sign(OFI)` strategy on out-of-sample sessions, with cost assumptions stated explicitly.

---

## 7. Build & reproduce

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

All artifacts (models, metrics, plots) are written to `mlruns/`. The MLflow UI is available via `mlflow ui`.

---

## 8. What I learned

> **TODO:** Fill in honestly at end of project. A few prompts to write against:

- Engine correctness: what bugs surfaced via the LOBSTER snapshot reconciliation? What did the first divergence look like?
- Feature engineering: which features ended up mattering? Were there features you expected to matter that didn't?
- Modeling: did the sequence model beat LightGBM by enough to justify the complexity? What was the train/inference cost trade-off?
- Evaluation: where did you almost introduce leakage? What checks caught it?
- Backtest vs classification: did the rank order of models change between offline metrics and the closed-loop test? Why?

---

## 9. What's next

The planned follow-up is **v2: a reinforcement-learning market-maker** that uses this engine as a Gymnasium environment. The supervised model in this project predicts price direction; the v2 agent will quote both sides of the book and learn a market-making policy via PPO, evaluated against the closed-form Avellaneda-Stoikov optimal market maker (2008) and the supervised policy from this project converted into a quote-skew rule. The engine, features, and evaluation harness here are designed to make that extension cheap. See `V2_PLAN.md` in the repo for the full design.

Other extensions, in roughly increasing order of effort:

1. **More tickers, more sessions.** With paid LOBSTER data or NASDAQ ITCH samples, retrain across regimes (high-vol vs. low-vol days, different sectors).
2. **Transformer architectures.** Try a temporal fusion transformer on the same feature representation.
3. **Multi-horizon / multi-task heads.** Predict K=10, 50, 100 jointly with a shared encoder.
4. **Latency-realistic backtest.** The closed-loop test currently assumes zero-latency reactions. Add a configurable delay and sweep over it to see how signal value decays.

---

## 10. References

- Avellaneda, M., Stoikov, S. (2008). *High-frequency trading in a limit order book.* Quantitative Finance, 8(3), 217–224.
- Cont, R., Kukanov, A., Stoikov, S. (2014). *The price impact of order book events.* Journal of Financial Econometrics, 12(1), 47–88.
- Gould, M. D., Porter, M. A., Williams, S., McDonald, M., Fenn, D. J., Howison, S. D. (2013). *Limit order books.* Quantitative Finance, 13(11), 1709–1742.
- Huang, R., Polak, T. (2011). *LOBSTER: Limit Order Book Reconstructor.* https://lobsterdata.com
- Kercheval, A. N., Zhang, Y. (2015). *Modelling high-frequency limit order book dynamics with support vector machines.* Quantitative Finance, 15(8), 1315–1329.
- Stoikov, S. (2018). *The micro-price: a high-frequency estimator of future prices.* Quantitative Finance, 18(12), 1959–1966.
- Zhang, Z., Zohren, S., Roberts, S. (2019). *DeepLOB: Deep convolutional neural networks for limit order books.* IEEE Transactions on Signal Processing, 67(11), 3001–3012.
