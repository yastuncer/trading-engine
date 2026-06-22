// Header Guard

#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <chrono>

// creating type alias--> nicknames for integer types
using Price = std::int64_t; // Price is the nickname for this type (signed integer)
using Quantity = std::int64_t;
using OrderId = std::uint64_t;
using Timestamp = std::uint64_t;

/*
 for enums --> a set of named constants that represent all
 possible states or categories of something
 i.e. side represents if its buying or selling
*/
enum class Side {
    Buy,
    Sell
};
enum class OrderType {
    Limit,
    Market
};
enum class OrderStatus {
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};

/*
struct --> the blueprint im using to hold group of related variables under one name
*/

// Order is like a form that gets filled out every time someone submits an order
struct Order {
    OrderId id; // unique # for to identify order
    Side side; // buy or sell
    OrderType type; // is it limit or market order type
    Price price; // at what price
    Quantity quantity; // how many shares in total
    Quantity filled_qty; // how many shares have been filled?
    OrderStatus status; // what state is the order in right now?
    Timestamp timestamp; // time of when it was submitted
    uint64_t user_id; // which user submitted it

    Quantity remaining() {
        return quantity - filled_qty;
    }
    bool is_filled() const {
        return filled_qty >= quantity;
    }

};

struct Trade {
    OrderId buy_order_id; // which buy order was involved
    OrderId sell_order_id; // which sell order was involved
    Price price; // what price did they trade at
    Quantity quantity; // how many shares traded
    Timestamp timestamp; // when did it happen
};

/*
When an Order comes thru, it looks thru the book for a matching Order on the other side. 
Once it finds one, it creates a Trade that references both orders by their IDs.
Orders == inputs
Trades == outputs
*/

// inline tells the compiler it's okay if multiple files see this definition, don't treat it as a duplicate

inline Timestamp now() { // timestamp is the return type
    // capturing the current point in time
    auto now = std::chrono::system_clock::now(); // auto figures out the type
    
    // calculate the duration since the epoch which was jan. 1, 1970
    auto duration = now.time_since_epoch();

    // convert to nanoseconds
    // duration_cast converts the duration into a specific unit & count() extracts thr raw number from it
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(); 

    return nanoseconds;
}

// converts 123.45 to 1234500
inline Price to_price(double value) {
    return static_cast<Price> (value * 10000); // casting value to price since it was double but we need to return Price type
}

// converts back to display, takes type Price and returns double --> 1234500 == 123.45
inline double from_price(Price value) {
    return static_cast<double> (value/10000.0);
}


#endif