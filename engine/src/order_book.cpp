#include "order_book.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

// Storage & Lookup
/*
Implements the methods that the header declared in order_book.hpp
This is the warehouse ( the data structure lives here)
storing orders that arrive (add method)
removing orderes when someone cancels them (cancel method)
telling what the best bids and ask are (best_bid, best_ask)
printing the current state for debugging
*/


// this method add only belongs to OrderBook's class
void OrderBook::add(Order order) { // take one parameter --> an Order struct thats copied by value into a lccal variable order
    /* first figure out which side of the book this order belongs to
    using a reference (the &) so we can modify the actual map*/


    if (order.side == Side::Buy) {
        // It's a bid. Find or create the price level in bids_.
        // map's operator[] does this automatically: if the key exists, it
        // returns a reference to the existing value; if not, it creates
        // a default-constructed PriceLevel and returns a reference to that.
        PriceLevel& level = bids_[order.price];
 
        // If we just created this level, its price field is still 0 (default).
        // Set it to the actual price.
        level.price = order.price;
 
        // Append the order to the back of the FIFO queue at this level.
        // push_back is O(1) on a deque.
        level.orders.push_back(order);
 
    } else {
        // It's an ask. Same logic, different map.
        PriceLevel& level = asks_[order.price];
        level.price = order.price;
        level.orders.push_back(order);
    }
 
    // Step 2: record where this order lives, so cancel() can find it later
    // without having to scan every price level.
    // We store the side (so we know which map to look in) and the price
    // (so we know which key in that map).
    order_locations_[order.id] = OrderLocation{order.side, order.price};

}

/*
Canceling an order --> cancel method
    1. look up where the order lives --> order_locations_ map tells me if its a bid or an ask and at what price
    2. handling the "doesnt exist" case --> if lookup fails then return std::nullopt and end the lookup
    3. find the best price level by using the side and price from step 1 and look up the right
        map (bids_ or asks_) and find the price level
    4. find the order within the price level's deque --> O(k) k is the number of orders at that price
    5. save a copy of the order before it's deleted so i can return it
    6. remove the order from the deque
    7. clean up the price level if it's empty
    8. remove the entry from order_locations_
    9. return the order i saved in step 5
*/


// this function returns an Order when found or nothing when not found
std::optional<Order> OrderBook::cancel(OrderId id) { // parameter is named id of type OrderId

    auto it = order_locations_.find(id); // iterator to a row in the map, .find searches for that key id and returns an iterator
    if (it == order_locations_.end()) {
        return std::nullopt;
    }
    Order cancelled;          // declared before, lives until end of function
    bool found_and_erased = false;  

    if (it->second.side == Side::Buy) { // getting that value at that row
        // look in bids_
        auto price_it = bids_.find(it->second.price);
        if (price_it == bids_.end()) {
            return std::nullopt;
        }
        PriceLevel& level = price_it->second;
        for (auto order_it = level.orders.begin(); order_it != level.orders.end(); ++order_it) {
            if (order_it->id == id) {
                cancelled = *order_it;
                level.orders.erase(order_it);
                found_and_erased = true;
                if (level.orders.empty()) {
                    bids_.erase(price_it);
                }
                break;
            }
        }

    } else {
        // look in asks_
        auto price_it = asks_.find(it->second.price);
        if (price_it == asks_.end()) {
            return std::nullopt;
        }
        PriceLevel& level = price_it->second;
        for (auto order_it = level.orders.begin(); order_it != level.orders.end(); ++order_it) {
            if (order_it->id == id) {
                cancelled = *order_it;
                level.orders.erase(order_it);
                found_and_erased = true;
                if (level.orders.empty()) {
                    asks_.erase(price_it);
                }
                break;
            }
        }
    }


    if (!found_and_erased) {
        return std::nullopt;  // location said it was there but the scan didn't find it
    }
    order_locations_.erase(id);  // remove the stale lookup entry


    return cancelled;
   

}



// Print function --> prints the order book
void OrderBook::print(int depth) const {

    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string RESET = "\033[0m"; // always reset at the end of colored region

    // check if order book is empty--> if bids_ & asks_ are empty 
    if (bids_.empty() && asks_.empty()) {
        std::cout << "(empty book)\n";
        return;
    }
    std::cout << std::fixed << std::setprecision(2);

    /*
    asks --> sorted in ascending order (lowest price first)
    but want to display the besk ask(lowest price) at the bottom section, right above the spread
    */

    std::cout << RED << "ASKS (lowest first):" << RESET << "\n";

    /* collect the price levels first & then print in reverse order
    creating an empty container to hold the collected price levels
    go thru asks from the beginning and add to container
    end after x depth entries or if the map runs out
    using pointers to store instead of copying every price level
    */

   // empty vector that holds pointers to PriceLevels
    std::vector<const PriceLevel*> top_asks;

    // loop thru the asks_ map, & for each entry add a pointer to its PriceLevel into top_asks
    int asks_count = 0;
    for (const auto& [price, level] : asks_) { // unpacking each map entry into 2 variables
        if (asks_count >= depth) {
            break; // stop once collected depth levels
        }
        top_asks.push_back(&level); // storing pointers
        asks_count++; // keeping track of how many entries added
    }


    // reverse loop
    for (auto it = top_asks.rbegin(); it != top_asks.rend(); ++it) {
        const PriceLevel* level = *it; // dereference the iterator to get the element of the vector into level
        Quantity total = 0; // every price level has its own total

        for (const Order& o : level->orders) { // o is every individual element being iterated in the price level's orders deque
            total += o.remaining(); // add this order's unfilled volume to the running total
        }
        std::cout << "  " << RED
          << "$" << from_price(level->price)
          << "  |  " << total << " units"
          << "    (" << level->orders.size() << " order"
          << (level->orders.size() == 1 ? "" : "s") << ")"
          << RESET << "\n";
    }

    /*
    Spread = best ask - best bid
    it's the gap
    */

    // making sure both maps are not empty then getting the best prices
    if (!bids_.empty() && !asks_.empty()) { // if bids is not empty && asks is not empty
        Price best_bid_price = bids_.begin()->first; // return entry to 1st entry (the price)
        Price best_ask_price = asks_.begin()->first;
        Price spread = best_ask_price - best_bid_price;
        std::cout << "\n ─── SPREAD: $" << from_price(spread) << " ───\n\n";
    } else {
        std::cout << "\n ─── (one side is empty, therefore no spread) ───\n\n";
    }


    std::cout << GREEN << "BIDS (highest first):" << RESET << "\n";

    int bids_count = 0;
    for (const auto& [price, level] : bids_) {
        if (bids_count >= depth) {
            break;
        }
        Quantity total = 0;
        for (const Order& o : level.orders) { // level.order is a direct reference not a pointer
            total += o.remaining();
        }
        std::cout << "  " << GREEN
          << "$" << from_price(level.price)
          << "  |  " << total << " units"
          << "    (" << level.orders.size() << " order"
          << (level.orders.size() == 1 ? "" : "s") << ")"
          << RESET << "\n";
        bids_count++;
    }




}


