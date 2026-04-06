# Trading Engine Project Plan

## Project Overview

**The big Picture:** Building a system that simulates how a stock
exchange works internally. Whenever I buys a stock through a broker such as Robinhood, Vanguard, etc., that order (request) needs to go to an exchange such as NASDAQ or NYSE. These exchanges have match engine systems that connect and find someone who would want to sell at my price.

**What I am building:** A full-stack trading engine simulating core exchange functionality — from a high-performance C++ matching core to a live React dashboard. Implements a limit order book with price-time priority, real trade execution, persistent storage, authenticated REST API, concurrent order ingestion, and real-time WebSocket streaming.

**Honest framing for resume/README:**
> "A full-stack limit order book engine with a C++ matching core, PostgreSQL persistence, authenticated REST API (FastAPI), concurrent order ingestion via thread pool, real-time WebSocket trade feed, and a React dashboard. Designed around how real exchanges work: single-threaded book for correctness, parallel ingestion for throughput."

**Timeline:** 7 weeks (~15 hrs/week)
**Goal:** Portfolio-ready, interview-ready, genuinely impressive at the new-grad/internship level

---

## Revised Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                        React Frontend                            │
│         (Order Book panel | Trade Blotter | Order Entry)        │
└────────────────────────────┬─────────────────────────────────────┘
                             │ REST + WebSocket
                             ▼
┌──────────────────────────────────────────────────────────────────┐
│                    FastAPI Service Layer                         │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│   │  REST API    │  │  JWT Auth    │  │  WebSocket Manager   │  │
│   │  /orders     │  │  Middleware  │  │  (pushes live feed)  │  │
│   │  /book       │  │              │  │                      │  │
│   │  /trades     │  └──────────────┘  └──────────────────────┘  │
│   └──────┬───────┘                                              │
└──────────┼───────────────────────────────────────────────────────┘
           │ Unix socket / subprocess
           ▼
┌──────────────────────────────────────────────────────────────────┐
│                    C++ Matching Engine                           │
│   ┌──────────────────────────────────────────────────────────┐   │
│   │                    Thread Pool                           │   │
│   │   (validate + queue orders concurrently)                 │   │
│   └──────────────────────┬───────────────────────────────────┘   │
│                          │                                       │
│   ┌──────────────────────▼───────────────────────────────────┐   │
│   │         Single-Threaded Matching Core                    │   │
│   │   ┌─────────────────┐       ┌─────────────────┐          │   │
│   │   │   Bids          │       │   Asks           │          │   │
│   │   │ descending      │       │ ascending        │          │   │
│   │   │  by price       │       │  by price        │          │   │
│   │   └─────────────────┘       └─────────────────┘          │   │
│   └──────────────────────────────────────────────────────────┘   │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                      PostgreSQL                                  │
│         orders table | trades table | users table                │
└──────────────────────────────────────────────────────────────────┘
```

---

## Full Features Checklist

### Phase 1 — C++ Core (Weeks 1–2)
- [ ] Limit order book with price-time priority
- [ ] Limit orders (buy/sell at specific price)
- [ ] Market orders (buy/sell at best available)
- [ ] Partial fill handling
- [ ] Trade feed output
- [ ] Book update output
- [ ] Complexity analysis in README

### Phase 2 — Database (Week 3)
- [ ] PostgreSQL schema (orders, trades, users)
- [ ] libpqxx integration in C++ engine
- [ ] Persist every order and trade execution
- [ ] Indexes on timestamp and order_id
- [ ] Connection pooling

### Phase 3 — REST API + Auth (Week 4)
- [ ] FastAPI service layer
- [ ] JWT authentication (login → token → protected routes)
- [ ] POST /orders
- [ ] GET /book
- [ ] GET /trades
- [ ] DELETE /orders/{id}
- [ ] Input validation with Pydantic

### Phase 4 — Concurrency (Week 5)
- [ ] Thread pool in C++ for order ingestion
- [ ] std::mutex / std::condition_variable
- [ ] Lock-free order ID counter (std::atomic)
- [ ] Engine stays single-threaded for correctness
- [ ] WebSocket server (live trade + book updates)

### Phase 5 — Frontend (Week 6)
- [ ] React + TypeScript dashboard
- [ ] Live order book panel (WebSocket)
- [ ] Trade blotter panel (WebSocket)
- [ ] Order entry form (calls REST API)
- [ ] Auth flow (login screen → JWT stored in memory)

### Phase 6 — Testing + Polish (Week 7)
- [ ] Google Test for C++ unit tests
- [ ] pytest for API integration tests
- [ ] Load test with k6 or locust (10k orders)
- [ ] Benchmarks (orders/sec)
- [ ] Clean README with architecture diagram

### Stretch Goals (After Week 7)
- [ ] Docker Compose (run entire stack with one command)
- [ ] Property-based tests
- [ ] Event sourcing / replay
- [ ] Rate limiting on API
- [ ] Candlestick chart on frontend

---

## Week 1: Setup + Core Types + Order Book

**Goal:** Working order book you can add orders to and inspect

### Days 1–2: Environment Setup (3–4 hrs)

**Tasks:**
- Create GitHub repo
- Set up folder structure
- Write a basic Makefile
- Work on types.hpp

**Folder structure (expanded for full project):**
```
trading-engine/
├── engine/                  ← C++ core
│   ├── src/
│   │   ├── main.cpp
│   │   ├── types.hpp
│   │   ├── order_book.hpp
│   │   ├── order_book.cpp
│   │   ├── matching_engine.hpp
│   │   ├── matching_engine.cpp
│   │   ├── thread_pool.hpp
│   │   └── db.hpp / db.cpp
│   ├── tests/
│   └── Makefile
├── api/                     ← Python FastAPI
│   ├── main.py
│   ├── auth.py
│   ├── models.py
│   ├── routes/
│   └── requirements.txt
├── frontend/                ← React + TypeScript
│   ├── src/
│   │   ├── App.tsx
│   │   ├── components/
│   │   │   ├── OrderBook.tsx
│   │   │   ├── TradeBlotter.tsx
│   │   │   └── OrderEntry.tsx
│   │   └── hooks/
│   └── package.json
├── db/
│   └── schema.sql
└── README.md
```

**Makefile basics you'll need:**
- Compiler: g++
- Flags: -std=c++17 -Wall -Wextra -O2
- Know how to define a target, dependencies, and build command

---

### Days 3–4: Core Types (4–5 hrs)

**Create types.hpp with:**

**Type aliases:**
- `Price` — use int64_t (fixed-point, see below)
- `Quantity` — int64_t
- `OrderId` — uint64_t
- `Timestamp` — uint64_t (nanoseconds since epoch)

**Enums:**
- `Side` — Buy, Sell
- `OrderType` — Limit, Market
- `OrderStatus` — New, PartiallyFilled, Filled, Cancelled, Rejected

**Structs:**
- `Order` — id, side, type, price, quantity, filled_qty, timestamp, user_id, orderstatus
  - Add a helper: `remaining()` returns quantity - filled_qty
  - Add a helper: `is_filled()` returns filled_qty >= quantity
- `Trade` — buy_order_id, sell_order_id, price, quantity, timestamp

**Helper functions:**
- `now()` — returns current timestamp using std::chrono
- `to_price(double)` — converts 123.45 to 1234500
- `from_price(Price)` — converts back for display

**Why fixed-point prices:**
Floating point has precision issues (0.1 + 0.2 != 0.3). Multiply all prices by 10000 and store as integers. $123.45 becomes 1234500. This avoids nasty bugs when comparing prices.

**Note:** Add `user_id` to Order now — you'll need it for the auth layer in Week 4.

---

### Days 5–7: Order Book (6–7 hrs)

**Data structures to use:**

For bids (buy orders) — need highest price first:
- `std::map<Price, PriceLevel, std::greater<Price>>`

For asks (sell orders) — need lowest price first:
- `std::map<Price, PriceLevel, std::less<Price>>`

Each PriceLevel holds:
- The price
- A `std::deque<Order>` — FIFO queue of orders at that price

For fast cancellation, track where each order lives:
- `std::unordered_map<OrderId, {Side, Price}>` — lookup order location by ID

**Methods to implement:**

`add(Order order)`
- Determine which side (bids or asks)
- Insert into the correct price level
- If price level doesn't exist, create it
- Track location in the lookup map

`cancel(OrderId id)` → returns the cancelled order or nothing
- Look up order location
- Find it in the price level's deque
- Remove it
- Clean up empty price levels
- Remove from lookup map

`best_bid()` / `best_ask()` → returns pointer to top price level
- Return iterator to begin() of the appropriate map
- Return nullopt if empty

`print(int depth)` — for debugging
- Show top N price levels on each side
- Display price and total quantity at each level

**Week 1 Checkpoint:**
- [ ] Code compiles and runs
- [ ] Can add orders via hardcoded test in main()
- [ ] Print shows bids descending, asks ascending
- [ ] Cancel works correctly

---

## Week 2: Matching Engine + Order Types

**Goal:** Orders actually match and produce trades

### Days 1–3: Basic Matching (5–6 hrs)

**Create MatchingEngine class that:**
- Owns an OrderBook
- Has a `process(Order)` method

**Matching logic for process():**

```
if incoming is a BUY:
    while (has asks) AND (best_ask price <= incoming price) AND (incoming has quantity left):
        execute trade against best ask

if incoming is a SELL:
    while (has bids) AND (best_bid price >= incoming price) AND (incoming has quantity left):
        execute trade against best bid

if incoming still has remaining quantity AND it's a limit order:
    add to book
```

**Executing a trade:**
- Fill quantity = min(incoming remaining, resting order remaining)
- Trade price = resting order's price (price improvement)
- Update filled_qty on both orders
- If resting order fully filled, remove from book
- Create Trade struct and add to results

**Return from process():**
- List of trades that executed
- Final status of incoming order
- The resting order if it was added to book

---

### Days 4–5: Market Orders + Partial Fills (4–5 hrs)

**Market orders:**
- Same matching logic but NO price check
- Match at whatever price is available
- If no liquidity, order disappears (not added to book)

**Partial fills:**
- An order for 100 might only fill 30 if that's all the liquidity
- Remaining 70 gets added to book (for limit orders)
- Track via filled_qty field

**Test scenarios to verify:**
- Buy 100 @ $10, book has Sell 30 @ $10 → fills 30, 70 rests
- Buy 100 @ $10, book has Sell 150 @ $10 → fills 100, 50 remains in book
- Market buy 50, book has Sell 20 @ $10, Sell 30 @ $11 → fills all 50 at two prices

---

### Days 6–7: Testing + Edge Cases (4–5 hrs)

**Test these scenarios:**
- Empty book + market order → no fill, order disappears
- Exact quantity match → both orders fully filled
- Partial fill → correct quantities on both sides
- Multiple orders at same price → FIFO (first order fills first)
- Cancel non-existent order → handle gracefully, don't crash
- Self-trade (same user_id buys and sells) → reject

**Week 2 Checkpoint:**
- [ ] Limit orders match correctly
- [ ] Market orders work
- [ ] Partial fills tracked correctly
- [ ] Trade output shows correct prices/quantities
- [ ] No crashes on edge cases

---

## Week 3: Database Layer (PostgreSQL)

**Goal:** Every order and trade is persisted. The engine has memory.

### Days 1–2: Schema Design + Setup (3–4 hrs)

**Install PostgreSQL locally and create the database:**
```bash
createdb trading_engine
psql trading_engine < db/schema.sql
```

**Create db/schema.sql:**
```sql
CREATE TABLE users (
    id          SERIAL PRIMARY KEY,
    username    VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    created_at  TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE orders (
    id          BIGSERIAL PRIMARY KEY,
    user_id     INTEGER REFERENCES users(id),
    side        VARCHAR(4) NOT NULL,       -- 'BUY' or 'SELL'
    type        VARCHAR(6) NOT NULL,       -- 'LIMIT' or 'MARKET'
    price       BIGINT,                    -- fixed-point, NULL for market orders
    quantity    BIGINT NOT NULL,
    filled_qty  BIGINT DEFAULT 0,
    status      VARCHAR(20) NOT NULL,
    created_at  TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE trades (
    id              BIGSERIAL PRIMARY KEY,
    buy_order_id    BIGINT REFERENCES orders(id),
    sell_order_id   BIGINT REFERENCES orders(id),
    price           BIGINT NOT NULL,
    quantity        BIGINT NOT NULL,
    executed_at     TIMESTAMPTZ DEFAULT NOW()
);

-- Indexes for fast queries
CREATE INDEX idx_orders_user_id   ON orders(user_id);
CREATE INDEX idx_orders_status    ON orders(status);
CREATE INDEX idx_trades_executed  ON trades(executed_at DESC);
CREATE INDEX idx_orders_created   ON orders(created_at DESC);
```

**Why these indexes:** Without them, `GET /trades` with 1M rows does a full table scan. With `executed_at DESC`, the DB hits an index and returns instantly. Interviewers love when you can explain this.

---

### Days 3–5: libpqxx Integration in C++ (5–6 hrs)

**Install libpqxx:**
```bash
# macOS
brew install libpqxx

# Ubuntu/Debian
sudo apt-get install libpqxx-dev
```

**Create db.hpp / db.cpp — a thin database wrapper:**

Key methods to implement:
- `save_order(const Order&)` → inserts into orders table, returns DB-assigned ID
- `update_order(OrderId, OrderStatus, Quantity filled)` → updates status + filled_qty
- `save_trade(const Trade&)` → inserts into trades table
- `get_recent_trades(int limit)` → returns last N trades

**Connection pooling concept:**
Don't open a new DB connection per order — that's slow. Instead, open a fixed pool of connections at startup (e.g., 4) and reuse them. This is a key backend concept you'll be asked about.

```cpp
// Sketch of what the DB class looks like
class Database {
    std::vector<pqxx::connection> pool;
    std::mutex pool_mutex;

public:
    Database(const std::string& conn_str, int pool_size);
    pqxx::connection& acquire();    // get a connection from pool
    void release(pqxx::connection&); // return it
    void save_order(const Order& o);
    void save_trade(const Trade& t);
};
```

**Important: wrap every DB write in a transaction.** If the engine saves an order but crashes before saving the trade, you have corrupted state. Transactions prevent this (ACID).

---

### Days 6–7: Wire DB into Matching Engine (3–4 hrs)

- Call `db.save_order()` when an order arrives
- Call `db.save_trade()` for each trade that executes
- Call `db.update_order()` when an order status changes
- Test: submit 10 orders via CLI, verify rows appear in psql

**Week 3 Checkpoint:**
- [ ] Schema created and migrates cleanly
- [ ] Orders persisted to DB on submission
- [ ] Trades persisted on execution
- [ ] Can query DB and see correct data
- [ ] No data loss on engine restart

---

## Week 4: REST API + Authentication

**Goal:** The engine is accessible over HTTP with secure auth

### Days 1–2: FastAPI Setup + Basic Routes (4–5 hrs)

**Why Python/FastAPI instead of C++ HTTP:**
Building a REST API in C++ is painful and slow. FastAPI gives you automatic docs, Pydantic validation, and async support in ~100 lines. The C++ engine becomes a subprocess or communicates over a Unix socket. This is realistic — real systems use the right language for each layer.

**Setup:**
```bash
cd api/
pip install fastapi uvicorn pydantic python-jose[cryptography] passlib psycopg2-binary
```

**Basic route structure (api/main.py):**
```python
from fastapi import FastAPI, Depends, HTTPException
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI()

app.add_middleware(CORSMiddleware, allow_origins=["http://localhost:5173"],
                   allow_methods=["*"], allow_headers=["*"])

@app.post("/orders")
async def create_order(order: OrderRequest, user=Depends(get_current_user)):
    # validate, forward to engine, return result
    ...

@app.get("/book")
async def get_book():
    # read current book state from DB or engine
    ...

@app.get("/trades")
async def get_trades(limit: int = 50, user=Depends(get_current_user)):
    ...

@app.delete("/orders/{order_id}")
async def cancel_order(order_id: int, user=Depends(get_current_user)):
    ...
```

**Pydantic models (api/models.py):**
```python
from pydantic import BaseModel, validator

class OrderRequest(BaseModel):
    side: str       # "BUY" or "SELL"
    type: str       # "LIMIT" or "MARKET"
    price: float | None = None
    quantity: int

    @validator('quantity')
    def quantity_positive(cls, v):
        if v <= 0:
            raise ValueError('quantity must be positive')
        return v

    @validator('price')
    def price_positive(cls, v):
        if v is not None and v <= 0:
            raise ValueError('price must be positive')
        return v
```

---

### Days 3–5: JWT Authentication (5–6 hrs)

**How JWT auth works (understand this cold for interviews):**
1. User POSTs username + password to `/auth/login`
2. Server verifies password hash, returns a signed JWT token
3. Client stores token (in memory — not localStorage for security)
4. Every subsequent request includes `Authorization: Bearer <token>`
5. Server verifies signature on every request via middleware

**Implement auth.py:**
```python
from jose import jwt
from passlib.context import CryptContext
from datetime import datetime, timedelta

SECRET_KEY = "your-secret-key-change-in-production"
ALGORITHM = "HS256"
TOKEN_EXPIRE_MINUTES = 60

pwd_context = CryptContext(schemes=["bcrypt"])

def hash_password(password: str) -> str:
    return pwd_context.hash(password)

def verify_password(plain: str, hashed: str) -> bool:
    return pwd_context.verify(plain, hashed)

def create_token(user_id: int) -> str:
    payload = {"sub": str(user_id),
               "exp": datetime.utcnow() + timedelta(minutes=TOKEN_EXPIRE_MINUTES)}
    return jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)

def decode_token(token: str) -> int:
    payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
    return int(payload["sub"])
```

**Auth routes:**
```python
@app.post("/auth/register")
async def register(credentials: LoginRequest):
    hashed = hash_password(credentials.password)
    # save user to DB
    ...

@app.post("/auth/login")
async def login(credentials: LoginRequest):
    user = get_user_by_username(credentials.username)
    if not user or not verify_password(credentials.password, user.password_hash):
        raise HTTPException(status_code=401, detail="Invalid credentials")
    return {"token": create_token(user.id)}

async def get_current_user(token: str = Depends(oauth2_scheme)):
    user_id = decode_token(token)
    return get_user_by_id(user_id)
```

**Security concepts you'll know after this:**
- Bcrypt hashing (why not MD5/SHA1)
- Token expiry and why it matters
- Why tokens live in memory not localStorage (XSS attacks)
- The difference between authentication (who are you) and authorization (what can you do)

---

### Days 6–7: Engine–API Bridge (3–4 hrs)

Connect the FastAPI layer to the C++ engine. The simplest approach:
- Engine writes its output (trades, book state) to the DB
- API reads from the DB
- For submitting orders: API writes to an `order_queue` table, engine polls it

This is called the **outbox pattern** — common in real distributed systems. More advanced alternative: Unix socket IPC (implement this as a stretch goal).

**Week 4 Checkpoint:**
- [ ] `POST /auth/login` returns a valid JWT
- [ ] Protected routes reject requests without token
- [ ] `POST /orders` validates input with Pydantic
- [ ] `GET /trades` returns real data from DB
- [ ] FastAPI docs auto-generated at `/docs`

---

## Week 5: Concurrency + WebSocket

**Goal:** The engine handles concurrent ingestion and pushes live updates

### Days 1–3: Thread Pool in C++ (5–6 hrs)

**Why a thread pool:**
Creating a new thread per order is expensive. A pool of N pre-created threads sit idle, waiting for work. When an order arrives, it's pushed onto a shared queue, and the next free thread picks it up.

**Implement thread_pool.hpp:**
```cpp
class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> task_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop = false;

public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();  // joins all threads

    template<typename F>
    void submit(F&& task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            task_queue.push(std::forward<F>(task));
        }
        cv.notify_one();  // wake a sleeping worker
    }
};
```

**Key design: what's concurrent vs single-threaded:**
- Concurrent (thread pool): order validation, DB writes, network parsing
- Single-threaded (matching core): the actual order book and matching logic

This is the real insight. The matching engine MUST be single-threaded for correctness — if two threads modify the book simultaneously, you get race conditions and incorrect fills. Real exchanges work exactly this way.

**Concepts you'll know cold:**
- Race conditions and why they're dangerous in financial systems
- Mutex vs spinlock tradeoffs
- `std::condition_variable` for efficient thread sleeping
- `std::atomic<uint64_t>` for lock-free order ID generation

---

### Days 4–7: WebSocket Server (5–6 hrs)

**Add WebSocket support to FastAPI:**
```python
from fastapi import WebSocket
from typing import List

class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.active_connections.append(ws)

    async def broadcast(self, message: dict):
        for connection in self.active_connections:
            await connection.send_json(message)

manager = ConnectionManager()

@app.websocket("/ws/trades")
async def trade_feed(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            await websocket.receive_text()  # keep alive
    except:
        manager.active_connections.remove(websocket)
```

**When a trade executes in the engine, broadcast it:**
```python
# Called from your trade execution path
await manager.broadcast({
    "type": "trade",
    "buy_order_id": trade.buy_order_id,
    "sell_order_id": trade.sell_order_id,
    "price": from_price(trade.price),
    "quantity": trade.quantity,
    "timestamp": trade.timestamp
})
```

**Week 5 Checkpoint:**
- [ ] Thread pool processes orders concurrently
- [ ] Matching core remains single-threaded
- [ ] No race conditions (run with ThreadSanitizer: `-fsanitize=thread`)
- [ ] WebSocket endpoint broadcasts trades in real time
- [ ] Can connect with wscat and see live trade feed

---

## Week 6: React Frontend

**Goal:** A live dashboard you can demo in an interview

### Days 1–2: Project Setup + Auth UI (3–4 hrs)

```bash
cd frontend/
npm create vite@latest . -- --template react-ts
npm install axios react-query
```

**Component structure:**
```
src/
├── App.tsx              ← routing, auth state
├── components/
│   ├── LoginForm.tsx    ← username/password → calls /auth/login
│   ├── OrderBook.tsx    ← live bids/asks via WebSocket
│   ├── TradeBlotter.tsx ← recent trades via WebSocket
│   └── OrderEntry.tsx   ← form to submit orders via REST
└── hooks/
    ├── useWebSocket.ts  ← manages WS connection + reconnect
    └── useAuth.ts       ← stores JWT in memory, exposes login/logout
```

**Auth flow:**
- Start on login screen
- On success, store JWT in React state (NOT localStorage — XSS risk)
- All subsequent API calls include `Authorization: Bearer <token>` header
- On 401 response, redirect to login

---

### Days 3–5: Core Dashboard Components (6–7 hrs)

**OrderBook.tsx — live bids and asks:**
```tsx
// Subscribes to /ws/book, renders two columns
// Bids (green) on left, Asks (red) on right
// Highlight row when price level changes
```

**TradeBlotter.tsx — recent executions:**
```tsx
// Subscribes to /ws/trades
// Scrolling table: Time | Buy Order | Sell Order | Price | Qty
// New trades slide in from top with a brief highlight animation
```

**OrderEntry.tsx — submit orders:**
```tsx
// Form fields: Side (BUY/SELL), Type (LIMIT/MARKET), Price, Quantity
// On submit: POST /orders with Bearer token
// Display success (order ID) or error message
// Disable price field when MARKET is selected
```

**Styling guidance:**
- Dark theme — trading terminals are always dark
- Green for bids/buys, red for asks/sells
- Monospace font for prices and quantities
- Don't over-engineer the UI — clean and functional beats flashy

---

### Days 6–7: WebSocket Integration + Polish (4–5 hrs)

**useWebSocket.ts hook:**
```typescript
function useWebSocket(url: string) {
    const [data, setData] = useState(null);
    const ws = useRef<WebSocket>(null);

    useEffect(() => {
        const connect = () => {
            ws.current = new WebSocket(url);
            ws.current.onmessage = (e) => setData(JSON.parse(e.data));
            ws.current.onclose = () => setTimeout(connect, 2000); // auto-reconnect
        };
        connect();
        return () => ws.current?.close();
    }, [url]);

    return data;
}
```

**Week 6 Checkpoint:**
- [ ] Login screen works with real JWT auth
- [ ] Order book updates live via WebSocket
- [ ] Trade blotter shows real executions as they happen
- [ ] Order entry submits and shows confirmation
- [ ] Demo-able end-to-end in under 2 minutes

---

## Week 7: Testing + Benchmarks + Polish

**Goal:** Portfolio-ready, defensible in every detail

### Days 1–2: C++ Unit Tests with Google Test (4–5 hrs)

**Install Google Test:**
```bash
# Add to Makefile as a dependency, or:
git clone https://github.com/google/googletest.git
```

**Tests to write (engine/tests/):**

`test_order_book.cpp`:
- Add bid → best_bid() returns correct price
- Add ask → best_ask() returns correct price
- Cancel existing order → removed from book
- Cancel non-existent order → no crash
- Multiple orders at same price → FIFO order maintained

`test_matching.cpp`:
- Limit buy matches limit sell → trade created
- Partial fill → correct remaining quantity
- Market order with empty book → no fill, order gone
- Market order sweeps multiple levels → correct total fill
- Self-trade prevention → order rejected

Run with: `./tests --gtest_output=xml:results.xml`

---

### Days 3–4: API Integration Tests with pytest (4–5 hrs)

```bash
pip install pytest httpx pytest-asyncio
```

**tests/test_api.py:**
```python
import pytest
from httpx import AsyncClient

@pytest.mark.asyncio
async def test_login_success():
    async with AsyncClient(base_url="http://localhost:8000") as client:
        resp = await client.post("/auth/login",
                                 json={"username": "test", "password": "test"})
    assert resp.status_code == 200
    assert "token" in resp.json()

@pytest.mark.asyncio
async def test_create_order_requires_auth():
    async with AsyncClient(base_url="http://localhost:8000") as client:
        resp = await client.post("/orders",
                                 json={"side": "BUY", "type": "LIMIT",
                                       "price": 100.0, "quantity": 10})
    assert resp.status_code == 401

@pytest.mark.asyncio
async def test_invalid_order_rejected():
    token = await get_test_token()
    async with AsyncClient(base_url="http://localhost:8000") as client:
        resp = await client.post("/orders",
                                 json={"side": "BUY", "type": "LIMIT",
                                       "price": -5.0, "quantity": 10},
                                 headers={"Authorization": f"Bearer {token}"})
    assert resp.status_code == 422
```

---

### Days 5–6: Load Test + Benchmarks (4–5 hrs)

**C++ benchmark (orders/sec):**
```cpp
auto start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < 100000; i++) {
    engine.process(generate_random_order());
}
auto end = std::chrono::high_resolution_clock::now();
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
std::cout << "Processed 100k orders in " << ms << "ms ("
          << (100000 * 1000 / ms) << " orders/sec)\n";
```

**API load test with k6:**
```javascript
// load_test.js
import http from 'k6/http';
export const options = { vus: 50, duration: '30s' };

export default function () {
    http.post('http://localhost:8000/orders',
        JSON.stringify({ side: 'BUY', type: 'LIMIT', price: 100.0, quantity: 10 }),
        { headers: { 'Authorization': `Bearer ${TOKEN}`,
                     'Content-Type': 'application/json' } });
}
```

**Scenarios to benchmark:**
- All limit orders (book builds up)
- All market orders (immediate execution)
- 50/50 mix
- Concurrent API load (50 virtual users)

---

### Day 7: README + Final Polish (3–4 hrs)

**README sections:**
1. Project description + honest framing
2. Architecture diagram (ASCII)
3. Tech stack table
4. Build + run instructions (should work from fresh clone)
5. API reference (endpoints, auth, example requests)
6. Complexity analysis table
7. Benchmark results
8. Design decisions (why each technology choice was made)
9. What you'd do differently at scale

**Complexity analysis:**

| Operation | Time | Why |
|-----------|------|-----|
| Add order | O(log P) | Map insertion, P = price levels |
| Cancel order | O(log P + K) | Find level + scan deque |
| Best bid/ask | O(1) | Map begin() |
| Match (per fill) | O(log P) | Remove filled level from map |
| DB write (order) | O(log N) | B-tree index insert |
| API auth check | O(1) | JWT signature verify, no DB hit |

**Week 7 Checkpoint:**
- [ ] Google Test suite passes with 0 failures
- [ ] pytest suite passes with 0 failures
- [ ] Load test shows throughput + latency numbers
- [ ] README explains every architectural decision
- [ ] Fresh clone + build works in under 5 minutes
- [ ] Can demo end-to-end live in an interview

---

## Data Structure Reference

### Order Book Visualization

**Bids (buy orders):** highest price first
```
Price 150.50: [Order#1, Order#4]   ← best bid
Price 150.25: [Order#2]
Price 150.00: [Order#3]
```

**Asks (sell orders):** lowest price first
```
Price 151.00: [Order#5]            ← best ask
Price 151.25: [Order#6, Order#7]
Price 152.00: [Order#8]
```

**Spread** = best ask - best bid = 151.00 - 150.50 = 0.50

---

## Tech Stack Summary

| Layer | Technology | Why |
|-------|-----------|-----|
| Matching core | C++17 | Performance, systems credibility |
| Database | PostgreSQL | Industry standard, ACID, great indexing |
| API layer | Python / FastAPI | Fast to build, async, auto-docs |
| Auth | JWT + bcrypt | Stateless, industry standard |
| Frontend | React + TypeScript | Most hireable frontend stack |
| Real-time | WebSocket | Native browser support, low overhead |
| Unit tests | Google Test | C++ standard |
| API tests | pytest | Python standard |
| Load tests | k6 | Modern, scriptable |

---

## Key Design Decisions

1. **Why single-threaded matching core?**
   Order books must process operations atomically. Two threads modifying the book simultaneously produce race conditions and incorrect fills. Real exchanges (NYSE, Nasdaq) use a single-threaded matching core with parallel ingestion — same pattern here.

2. **Why separate API layer instead of C++ HTTP?**
   C++ HTTP libraries are painful. FastAPI gives you validation, auth middleware, auto-generated docs, and async support in a fraction of the code. Real systems use the right tool for each layer.

3. **Why JWT over sessions?**
   JWT is stateless — the server doesn't store session state, which means it scales horizontally. Session cookies require a shared session store (Redis, etc.) across instances.

4. **Why fixed-point prices?**
   Floating point comparison bugs. `0.1 + 0.2 != 0.3` in IEEE 754. Money must be exact.

5. **Why PostgreSQL over something simpler?**
   ACID guarantees matter for financial data. SQLite has no connection pooling. PostgreSQL is what you'll use in real jobs.

---

## Interview Talking Points

**C++ core:**
- Why std::map for price levels? O(log n), automatically sorted, supports custom comparator
- Why std::deque at each level? O(1) push_back and pop_front for FIFO
- Why thread pool + single-threaded core? Parallelism where safe, serialized where correctness matters

**Database:**
- Why indexes on timestamp and order_id? Query pattern analysis — most reads are time-ordered
- What does ACID mean and why does it matter here? Atomicity prevents partial trade writes
- What is connection pooling and why does it matter? Opening a new connection per order is O(100ms)

**API + Auth:**
- How does JWT work? Signed payload, stateless, expiry
- Why bcrypt over SHA256? Bcrypt is slow by design — makes brute force expensive
- What is CORS and why did you configure it? Cross-origin request security

**What would you do differently at scale?**
- Lock-free structures (atomic operations instead of mutexes)
- Memory pooling (avoid heap allocation per order)
- Kernel bypass networking (DPDK, bypass OS TCP stack)
- Separate read/write DB replicas
- Redis for real-time book state (avoid DB reads on hot path)

---

## What You'll Learn Week by Week

| Week | Core Concepts |
|------|--------------|
| 1 | std::map with custom comparator, std::deque, std::optional, fixed-point math |
| 2 | Matching algorithms, price-time priority, partial fill tracking |
| 3 | SQL schema design, indexes, ACID transactions, connection pooling, libpqxx |
| 4 | REST API design, Pydantic validation, JWT auth, bcrypt, middleware |
| 5 | Thread pools, std::mutex, std::condition_variable, std::atomic, WebSocket |
| 6 | React hooks, TypeScript, WebSocket client, auth state management |
| 7 | Unit testing patterns, integration testing, load testing, benchmarking |

---

## After Week 7 (Stretch Goals)

**Docker Compose — highest value add:**
```yaml
services:
  engine:   # C++ binary
  api:      # FastAPI
  db:       # PostgreSQL
  frontend: # React build served by nginx
```
One command to run the whole stack: `docker-compose up`. Interviewers love this.

**Other stretch goals:**
- Rate limiting (prevent order spam per user)
- Event sourcing (store every state change, replay to reconstruct book)
- Candlestick chart on frontend (OHLCV aggregation in DB)
- Order types: IOC (Immediate or Cancel), FOK (Fill or Kill)
- Redis for real-time book state caching

---

## Dev Log Template

Keep notes as you build. Interview gold.

```
## [Date]

**What I built:**
-

**Problems I hit:**
-

**How I solved it:**
-

**What I learned:**
-

**What I'd do differently:**
-
```

---

## Resources

**C++ Core:**
- [std::map reference](https://en.cppreference.com/w/cpp/container/map)
- [std::chrono reference](https://en.cppreference.com/w/cpp/chrono)
- [std::atomic reference](https://en.cppreference.com/w/cpp/atomic/atomic)
- [Google Test docs](https://google.github.io/googletest/)

**Database:**
- [libpqxx docs](https://libpqxx.readthedocs.io/)
- [PostgreSQL index types](https://www.postgresql.org/docs/current/indexes-types.html)

**API + Auth:**
- [FastAPI docs](https://fastapi.tiangolo.com/)
- [JWT explained](https://jwt.io/introduction)
- [python-jose](https://python-jose.readthedocs.io/)

**Frontend:**
- [React docs](https://react.dev/)
- [Vite setup](https://vitejs.dev/guide/)

**Load Testing:**
- [k6 docs](https://k6.io/docs/)

**Domain:**
- [How Limit Order Books Work](https://www.investopedia.com/terms/l/limitorderbook.asp)
- [Price-Time Priority](https://www.investopedia.com/terms/p/price-priority-rule.asp)
