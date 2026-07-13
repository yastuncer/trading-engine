// Header Guard, declares the MatchingEngine class

/*
Matching Logic, uses the order book but contains the logic.
This is the warehouse manager
Checking whether an incoming buy order matches a resting sell order
Execute trades
Update quantities when a partial fill happens
Decide what happens to leftover quantity
*/

#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <iostream>
#include "types.hpp"
#include <map>
#include <deque>
#include <unordered_map>
#include <optional>
#include <vector>
#include "order_book.hpp"
#include "matching_engine.hpp"



struct MatchResult {
    std::vector<Trade> trades; // list to store as many Trade objects needed
    OrderStatus status;// final state of incoming order after processing
    std::optional<Order> resting_order; // an order that may or may not exist
};


class MatchingEngine {
    public:
        MatchResult process(const Order& order); // getting a const reference to the caller's Order, not copy (wasteful cuz Order has 9 fields)
        const OrderBook& get_book() const; // returning a const reference to the OrderBook

    private:
        OrderBook book_;

};


/*
 MatchResult is the engine's receipt for processing a single incoming order.
 it does:
    - trades:  all executions that happened as a result of this order
    - status:  the final state of the incoming order (Filled, PartiallyFilled, New, Rejected, etc.)
    - order:   the resting order added to the book, if any (empty if fully filled or market order with no liquidity)
 */





#endif