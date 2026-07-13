import os

from dotenv import load_dotenv
import databento as db

load_dotenv()
api_key = os.getenv("DATABENTO_API_KEY")

client = db.Historical(api_key)

# cost checking
cost = client.metadata.get_cost(
    dataset="XNAS.ITCH",
    symbols=["AAPL"],
    schema="mbo",
    start="2026-06-01",
    end="2026-06-02",
)

print(cost)