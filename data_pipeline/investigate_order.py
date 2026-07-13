import databento as db
import pandas as pd

pd.set_option("display.max_columns", None)
pd.set_option("display.width", 200)

# load the same file
dbn_store = db.DBNStore.from_file("aapl_20260601.dbn.zst")
df = dbn_store.to_df()

# order IDs we saw hit but not in our book
suspect_ids = [459685, 360289, 62189]

for oid in suspect_ids:
    print(f"\n=== ORDER {oid} ===")
    events = df[df['order_id'] == oid]
    print(f"Total events referencing this order: {len(events)}")
    print(f"Actions: {events['action'].value_counts().to_dict()}")
    print(f"First 5 events:")
    print(events.head())


print("\n=== C EVENTS BY SIZE ===")
c_events = df[df['action'] == 'C']
print(f"Total C events: {len(c_events):,}")
print(f"Size distribution:")
print(c_events['size'].describe())
print(f"\nFirst 10 C events with non-zero size (potential partials):")
print(c_events.head(10))

o = df[df['order_id'] == 459685]
print("459685 events:")
print(o[['action', 'size', 'ts_event']].to_string())
print(f"\nSum of F sizes: {o[o['action'] == 'F']['size'].sum()}")
print(f"Sum of C sizes: {o[o['action'] == 'C']['size'].sum()}")

o = df[df['order_id'] == 594621]
print("\n594621 events:")
print(o[['action', 'size', 'ts_event']].to_string())