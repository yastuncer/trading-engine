

import sys
sys.path.insert(0, "../engine")   # so Python can find the .so

import trading_engine

import databento as db
import pandas as pd

'''
id         = synthetic_id (parser-generated)
side       = flip(T.side) → Buy or Sell
type       = Market (by convention)
price      = worst price in cluster (or 0 sentinel)
quantity   = sum of F sizes
filled_qty = 0
timestamp  = ts_event
'''

# load the dbn file into a DBNStore object
dbn_store = db.DBNStore.from_file("aapl_20260601.dbn.zst")
# convert the DBNStore object to a pandas DataFrame
df = dbn_store.to_df()


engine = trading_engine.MatchingEngine()
book = engine.get_book() 

next_synthetic_id = 10000000000 # my counter for the syntetic IDs
current_cluster = [] # buffered Trade/Fill events for the current cluster
last_sequence = None # sequence number of the last trade/fill event i saw

skipped_clusters = 0
skipped_actions = {'F_only': 0, 'T_only': 0, 'other': 0}


def close_cluster(cluster):
    """ Reconstruct the aggressor from a T/F cluster and process it thru the engine
    1. take all the buffered t/f events sitting in current_cluster
    2. figure out what the original aggressor order looked like (side, price, quantity, timestamp)
    3. build a synthetic Order representing the aggresor
    4. feed it to the engine so the engine can actually match it against the book
    """
    global next_synthetic_id
    t_side = None

    # finding the T event
    for event in cluster:
        if event.action == 'T': 
            t_side = event.side
            break
    if t_side is None:
        global skipped_clusters
        skipped_clusters += 1
        actions = [e.action for e in cluster]
        if actions == ['F']:
            skipped_actions['F_only'] += 1
        elif all(a == 'T' for a in actions):
            skipped_actions['T_only'] += 1
        else:
            skipped_actions['other'] += 1
        return
    # flipping it
    if t_side == 'A':
        aggressor_side = trading_engine.Side.Buy
    else: # t_side == 'B
        aggressor_side = trading_engine.Side.Sell

    t_prices = []   # note the plural — it's a list
    for event in cluster:
        if event.action == 'T':
            t_prices.append(event.price)
    if aggressor_side == trading_engine.Side.Buy:
        worst_price = max(t_prices)
    else:
        worst_price = min(t_prices)
    aggressor_price = int(worst_price * 10000)

    aggressor_quantity = 0
    for event in cluster:
        if event.action == 'F':
            aggressor_quantity += event.size

    aggressor_id = next_synthetic_id
    next_synthetic_id += 1

    aggressor_type = trading_engine.OrderType.Market

    aggressor_timestamp = cluster[0].ts_event.value


    aggressor = trading_engine.Order()
    aggressor.id = aggressor_id
    aggressor.side = aggressor_side
    aggressor.type = aggressor_type
    aggressor.price = aggressor_price
    aggressor.quantity = aggressor_quantity
    aggressor.filled_qty = 0
    aggressor.status = trading_engine.OrderStatus.New
    aggressor.timestamp = aggressor_timestamp

    result = engine.process(aggressor)



# looping through every row of the dataframe and translating the data to get processed through my engine

for i, row in enumerate(df.itertuples()):
    if i % 500000 == 0:
        print(f"Processing row {i:,} / {len(df):,}")

    if row.action == 'R': # for resetting
        if current_cluster:
            close_cluster(current_cluster)
            current_cluster = []
            last_sequence = None
        book.clear()

    elif row.action == 'A': # for adding an order
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

    elif row.action == 'C': # for cancels
        if current_cluster:
            close_cluster(current_cluster)
            current_cluster = []
            last_sequence = None
        book.cancel(row.order_id) # orderID is directly from the row itself

    elif row.action == 'T' or row.action == 'F':
        # a continuation
        if len(current_cluster) > 0 and row.sequence == last_sequence + 1: 
            current_cluster.append(row)
            last_sequence = row.sequence
        else:
            # not a continuation
            if current_cluster:
                close_cluster(current_cluster)
                current_cluster = []
                last_sequence = None
            # starting fresh with this event (append + update)
            current_cluster.append(row)
            last_sequence = row.sequence

# after the loop — catch any leftover cluster from the final events       
if current_cluster:
        close_cluster(current_cluster)
        current_cluster = []
        last_sequence = None


print(f"\nParser finished. Processed {len(df):,} events.")
print(f"Handed out {next_synthetic_id - 10_000_000_000:,} synthetic aggressor IDs.")
book.print(10)  # show top 10 levels on each side

print(f"\nSkipped {skipped_clusters:,} clusters with no T event")
print(f"  F-only: {skipped_actions['F_only']:,}")
print(f"  T-only: {skipped_actions['T_only']:,}")
print(f"  Other: {skipped_actions['other']:,}")

