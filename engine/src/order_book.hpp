/*
Declaration of what the OrderBook class is
(members, methods signatures)
*/


#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP


#include "types.hpp"
#include <map> // std::map — sorted key-value store
#include <deque> // std::deque — double-ended queue
#include <unordered_map> // std::unordered_map — fast hash map
#include <optional> // std::optional — a value that might not exist


struct PriceLevel {
    Price price; // the price this level represents
    std::deque<Order> orders; // all orders sitting at this price, in FIFO order
};

/*
PriceLevel {
    price: 150.50
    orders: [ Order#1, Order#2, Order#4 ]   ← this is the deque
                 ↑                    ↑
              front                  back
           (fills first)        (arrived last)
}
*/

/* std::deque is a data structure to hold the orders placed (FIFO order), 
Order (Order objects) is the type of thing stored inside the deque
orders is the variable name. it is the actual deque living inside each PriceLevel.
*/

struct OrderLocation {
    Side side; // is this order a bid or an ask
    Price price; // what price level is it
};


class OrderBook {

    public:
        void add(Order order); // add a new order to the Order book
        std::optional<Order> cancel(OrderId id); // remove an order, return if its found else nothing
        std::optional<PriceLevel*> best_bid(); // top of bids (highest asks), or nothing if empty
        std::optional<PriceLevel*> best_ask(); // top of asks (lowest price), '''
        void print(int depth = 5) const; // print top N levels for debugging; const = doesn't modify the book


    private:
        // bids: highest price first
        std::map<Price, PriceLevel, std::greater<Price>> bids_; // sorted map of price, std::greater sorts highest price first

        // asks: lowest price first
        std::map<Price, PriceLevel, std::less<Price>> asks_; // sorted map of price, std::less sorts by lowest price first

        // fast lookup: order id --> where it lives in the book
        std::unordered_map<OrderId, OrderLocation> order_locations_; // hash map from order ID → its location (side + price)
};


#endif