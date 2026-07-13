import os

from dotenv import load_dotenv
import databento as db


load_dotenv()
api_key = os.getenv("DATABENTO_API_KEY")
client = db.Historical(api_key)

# data-pulling
data = client.timeseries.get_range(
    dataset ="XNAS.ITCH",
    symbols =["AAPL"],
    schema ="mbo",
    start ="2026-06-01",
    end ="2026-06-02",
)

# saves in databento's native compressed binary format 
data.to_file("aapl_20260601.dbn.zst")
print("saved")