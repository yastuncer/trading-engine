import databento as db
import pandas as pd


# load the dbn file into a DBNStore object
dbn_store = db.DBNStore.from_file("aapl_20260601.dbn.zst")
# convert the DBNStore object to a pandas DataFrame
df = dbn_store.to_df()

pd.set_option("display.max_columns", None)
print(df.shape) # size of the data frame as a tuple --> (rows, columns)
print(df.head(20)) # shows the first n rows
print(df.columns.tolist()) 

print(df['action'].value_counts())
print(df['side'].value_counts())

print("\n=== TRADE EVENTS ===")
print(df[df['action'] == 'T'].head(10))

print("\n=== FILL EVENTS ===")
print(df[df['action'] == 'F'].head(10))

print("\n=== MODIFY EVENTS ===")
print(df[df['action'] == 'M'].head(10))

print(df['action'].unique())
