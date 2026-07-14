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
#include <vector> // std::vector — dynamic array (for preview_match results)


// PriceLevel represents a bucket that holds all orders at one particular price
struct PriceLevel {
    Price price; // the price this level represents
    std::deque<Order> orders; // all orders sitting at this price, in FIFO order in a deque container
};


struct OrderLocation {
    Side side; // is this order a bid or an ask
    Price price; // what price level is it
};


class OrderBook {

    public:
        void add(Order order); // add a new order to the Order book
        std::optional<Order> cancel(OrderId id); // remove an order, return if its found else nothing
        const PriceLevel* best_bid() const; // top of bids (highest asks), or nothing if empty
        const PriceLevel* best_ask() const; // top of asks (lowest price), '''
        void print(int depth = 5) const; // print top N levels (default value) for debugging; const = doesn't modify the book
        bool apply_fill(OrderId id, Quantity fill_qty);
        bool reduce_order(OrderId id, Quantity by_qty); // shrink an order by by_qty; remove it if that empties it. returns if found
        // read-only walk: which resting orders would `incoming` consume, in price-time
        // priority, WITHOUT touching the book. cheap (visits only the orders it fills).
        std::vector<Trade> preview_match(const Order& incoming) const;
        void clear(); // for reseting the market at the start of the day



    private:
        // bids: highest price first (sorted that way cuz of greater)
        // bids_ is a map, the map contains key&value (keys are Prices, && values are PriceLevels)
        std::map<Price, PriceLevel, std::greater<Price>> bids_; 

        // asks: lowest price first (sorted that way cuz of less)
        // asks_ is a map that holds key&value pairs (key--> Price && value --> PriceLevel)
        std::map<Price, PriceLevel, std::less<Price>> asks_; 

        // fast lookup: order id --> where it lives in the book
        std::unordered_map<OrderId, OrderLocation> order_locations_; // hash map from order ID → its location (side + price)
};


#endif



