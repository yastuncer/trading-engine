import sys
sys.path.insert(0, "../engine")   # so Python can find the .so

import trading_engine

print("Import successful!")
print("Available in trading_engine:", dir(trading_engine))


# create a book
book = trading_engine.OrderBook()

# make an order
order = trading_engine.Order()
order.id = 1
order.side = trading_engine.Side.Buy
order.type = trading_engine.OrderType.Limit
order.price = 3000000    # $300.00 in fixed-point
order.quantity = 100
order.filled_qty = 0
order.status = trading_engine.OrderStatus.New
order.timestamp = 1000000000

# add it
book.add(order)

# print the book to verify
book.print()