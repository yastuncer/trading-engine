import sys
sys.path.insert(0, "../engine")

import trading_engine
import databento as db
import pandas as pd

'''
Aggressor Order {
    id         = synthetic_id (parser-generated)
    side       = flip(T.side) → Buy or Sell
    type       = Market (by convention)
    price      = worst price in cluster (or 0 sentinel)
    quantity   = sum of F sizes
    filled_qty = 0
    timestamp  = ts_event
}
'''

# load the dbn file
dbn_store = db.DBNStore.from_file("aapl_20260601.dbn.zst")
df = dbn_store.to_df()

df = dbn_store.to_df()

# Mark C events that are fill-cleanups (preceded by an F for same order_id at same ts_event)
df['prev_action'] = df['action'].shift(1)
df['prev_order_id'] = df['order_id'].shift(1)
df['prev_ts_event'] = df['ts_event'].shift(1)
df['is_fill_cleanup'] = (
    (df['action'] == 'C') &
    (df['prev_action'] == 'F') &
    (df['prev_order_id'] == df['order_id']) &
    (df['prev_ts_event'] == df['ts_event'])
)

engine = trading_engine.MatchingEngine()
book = engine.get_book()

# state variables
next_synthetic_id = 10_000_000_000
current_cluster = []
last_sequence = None

# counters for diagnostics
applied_fills_count = 0
apply_fill_failures = 0
clusters_matched = 0
clusters_mismatched = 0
total_quantity_delta = 0
orders_matched = 0
orders_mismatched = 0
clusters_with_zero_trades = 0
mismatch_debug_count = 0
skipped_clusters = 0
skipped_actions = {'F_only': 0, 'T_only': 0, 'other': 0}


def apply_truth(cluster):
    """Advance the authoritative book to ground truth by applying each Databento F
    to the order it actually names. This — not the engine's own matching — is what
    keeps the book in sync with reality."""
    global applied_fills_count, apply_fill_failures
    for event in cluster:
        if event.action == 'F':
            if book.apply_fill(event.order_id, event.size):
                applied_fills_count += 1
            else:
                apply_fill_failures += 1


def close_cluster(cluster):
    """Reconstruct the aggressor from a T/F cluster, compare what our engine WOULD
    fill (engine.simulate, read-only) against Databento's actual fills, then advance
    the book to ground truth by applying those actual fills."""
    global next_synthetic_id, skipped_clusters, applied_fills_count, apply_fill_failures
    global orders_matched, orders_mismatched
    global clusters_matched, clusters_mismatched, total_quantity_delta
    global mismatch_debug_count, clusters_with_zero_trades

    t_side = None

    # find a T event
    for event in cluster:
        if event.action == 'T':
            t_side = event.side
            break

    # F-only cluster — no aggressor to reconstruct, just advance the book to truth
    if t_side is None:
        skipped_clusters += 1
        skipped_actions['F_only' if len(cluster) == 1 else 'other'] += 1
        apply_truth(cluster)
        return

    # off-book prints (N-side): no on-book aggressor to reconcile, but still apply
    # any fills so the book stays truthful
    if t_side == 'N':
        apply_truth(cluster)
        return

    if t_side == 'A':
        aggressor_side = trading_engine.Side.Sell   # T side=A means the aggressor was on the ask side (seller)
    else:  # 'B'
        aggressor_side = trading_engine.Side.Buy    # T side=B means the aggressor was on the bid side (buyer)

    # collect T prices to pick the worst
    t_prices = [event.price for event in cluster if event.action == 'T']
    if aggressor_side == trading_engine.Side.Buy:
        worst_price = max(t_prices)
    else:
        worst_price = min(t_prices)
    aggressor_price = int(worst_price * 10000)

    # aggressor quantity = sum of F sizes
    aggressor_quantity = sum(event.size for event in cluster if event.action == 'F')

    aggressor_id = next_synthetic_id
    next_synthetic_id += 1

    aggressor = trading_engine.Order()
    aggressor.id = aggressor_id
    aggressor.side = aggressor_side
    aggressor.type = trading_engine.OrderType.Market
    aggressor.price = aggressor_price
    aggressor.quantity = aggressor_quantity
    aggressor.filled_qty = 0
    aggressor.status = trading_engine.OrderStatus.New
    aggressor.timestamp = cluster[0].ts_event.value

    # capture book state BEFORE processing (read-only, safe)
    pre_best_ask = book.best_ask()
    pre_best_bid = book.best_bid()
    pre_best_ask_price = pre_best_ask.price / 10000 if pre_best_ask else None
    pre_best_bid_price = pre_best_bid.price / 10000 if pre_best_bid else None

    # READ-ONLY: what would our engine fill against the current (truthful) book?
    # simulate() does not mutate the book — the book is advanced by apply_truth() below.
    result = engine.simulate(aggressor)

    # what did Databento hit?
    databento_hits = set()
    for event in cluster:
        if event.action == 'F':
            databento_hits.add((event.order_id, event.size))

    # what did our engine hit?
    engine_hits = set()
    for trade in result.trades:
        if aggressor_side == trading_engine.Side.Buy:
            engine_hits.add((trade.sell_order_id, trade.quantity))
        else:
            engine_hits.add((trade.buy_order_id, trade.quantity))

    if databento_hits == engine_hits:
        orders_matched += 1
    else:
        orders_mismatched += 1

    engine_qty = sum(trade.quantity for trade in result.trades)
    databento_qty = sum(event.size for event in cluster if event.action == 'F')
    if engine_qty == databento_qty:
        clusters_matched += 1
    else:
        clusters_mismatched += 1
        total_quantity_delta += abs(engine_qty - databento_qty)

    # debug the first 5 mismatched clusters
    if databento_hits != engine_hits and mismatch_debug_count < 5:
        mismatch_debug_count += 1
        print(f"\n=== MISMATCHED CLUSTER #{mismatch_debug_count} ===")
        print(f"Cluster events:")
        for event in cluster:
            print(f"  {event.action} side={event.side} price={event.price} size={event.size} order_id={event.order_id}")
        print(f"Aggressor: side={aggressor_side}, quantity={aggressor_quantity}, price=${aggressor_price/10000:.2f}")
        print(f"Databento hits: {databento_hits}")
        print(f"Engine trades:")
        for trade in result.trades:
            print(f"  buy_id={trade.buy_order_id} sell_id={trade.sell_order_id} price=${trade.price/10000:.2f} qty={trade.quantity}")
        print(f"Engine hits: {engine_hits}")
        print(f"Book state BEFORE this aggressor was processed:")
        if pre_best_ask_price is not None:
            print(f"  Best ask: ${pre_best_ask_price:.2f}")
        else:
            print(f"  Best ask: EMPTY")
        if pre_best_bid_price is not None:
            print(f"  Best bid: ${pre_best_bid_price:.2f}")
        else:
            print(f"  Best bid: EMPTY")
        print("========================================\n")

    if len(result.trades) == 0:
        clusters_with_zero_trades += 1

    # advance the authoritative book to ground truth (done AFTER simulate, so the
    # engine was measured against the pre-fill book state)
    apply_truth(cluster)


# main loop
for i, row in enumerate(df.itertuples()):
    if i % 500_000 == 0:
        print(f"Processing row {i:,} / {len(df):,}")

    if row.action == 'R':
        if current_cluster:
            close_cluster(current_cluster)
            current_cluster = []
            last_sequence = None
        book.clear()

    elif row.action == 'A':
        if current_cluster:
            close_cluster(current_cluster)
            current_cluster = []
            last_sequence = None
        order = trading_engine.Order()
        order.id = row.order_id
        order.side = trading_engine.Side.Buy if row.side == 'B' else trading_engine.Side.Sell
        order.type = trading_engine.OrderType.Limit
        order.price = int(row.price * 10000)
        order.quantity = row.size
        order.filled_qty = 0
        order.status = trading_engine.OrderStatus.New
        order.timestamp = row.ts_event.value
        book.add(order)

    elif row.action == 'C':
        if current_cluster:
            close_cluster(current_cluster)
            current_cluster = []
            last_sequence = None
        if not row.is_fill_cleanup:
            # Standalone C: shrink the order by the C's size. A full delete has
            # size == remaining, so reduce_order takes it to 0 and removes it; a
            # partial cancel just shrinks it. Safe now that fills are applied to
            # named orders (below), so the book's remaining tracks ground truth.
            book.reduce_order(row.order_id, row.size)

    elif row.action == 'T' or row.action == 'F':
        if len(current_cluster) > 0 and row.ts_event == current_cluster[0].ts_event:
            current_cluster.append(row)
        else:
            if current_cluster:
                close_cluster(current_cluster)
                current_cluster = []
            current_cluster.append(row)

# leftover cluster after the loop
if current_cluster:
    close_cluster(current_cluster)
    current_cluster = []
    last_sequence = None


# summary
print(f"\nParser finished. Processed {len(df):,} events.")
print(f"Handed out {next_synthetic_id - 10_000_000_000:,} synthetic aggressor IDs.")
book.print(10)

print(f"\nSkipped {skipped_clusters:,} clusters with no T event")
print(f"  F-only: {skipped_actions['F_only']:,}")
print(f"  T-only: {skipped_actions['T_only']:,}")
print(f"  Other: {skipped_actions['other']:,}")
print(f"  Applied {applied_fills_count:,} direct fills successfully")
print(f"  Failed to apply {apply_fill_failures:,} fills (order not found)")

total = clusters_matched + clusters_mismatched
print(f"\nReconciliation: {clusters_matched:,} / {total:,} clusters match ({100*clusters_matched/total:.1f}%)")
print(f"Total quantity delta on mismatches: {total_quantity_delta:,} shares")

total = orders_matched + orders_mismatched
print(f"\nOrder-level reconciliation: {orders_matched:,} / {total:,} clusters have identical resting-order hits ({100*orders_matched/total:.1f}%)")
print(f"  Clusters where engine produced 0 trades: {clusters_with_zero_trades:,}")