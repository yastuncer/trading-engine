#include <iostream>
#include "types.hpp"
#include "matching_engine.hpp"

int main() {
    MatchingEngine engine;
    
    // Set up the book with some resting sell orders
    engine.process({1, Side::Sell, OrderType::Limit, to_price(151.00), 30, 0,
                    OrderStatus::New, now(), 100});
    engine.process({2, Side::Sell, OrderType::Limit, to_price(151.50), 60, 0,
                    OrderStatus::New, now(), 100});
    
    std::cout << "Book after adding two sell orders:\n";
    engine.get_book().print();
    
    // Now send in a buy order that should match
    std::cout << "\n--- Sending buy 50 @ $151.20 ---\n";
    MatchResult result = engine.process({3, Side::Buy, OrderType::Limit, to_price(151.20), 50, 0,
                                         OrderStatus::New, now(), 200});
    
    // Print what happened
    std::cout << "Trades executed: " << result.trades.size() << "\n";
    for (const Trade& t : result.trades) {
        std::cout << "  Trade: buy#" << t.buy_order_id 
                  << " <-> sell#" << t.sell_order_id
                  << " at $" << from_price(t.price)
                  << " for " << t.quantity << " units\n";
    }
    
    std::cout << "Incoming order status: ";
    switch (result.status) {
        case OrderStatus::Filled: std::cout << "Filled\n"; break;
        case OrderStatus::PartiallyFilled: std::cout << "PartiallyFilled\n"; break;
        case OrderStatus::New: std::cout << "New\n"; break;
        default: std::cout << "Other\n"; break;
    }
    
    std::cout << "\nBook after the buy:\n";
    engine.get_book().print();


    std::cout << "\n--- Sending buy 60 @ $151.50 (should fully fill) ---\n";
    MatchResult result2 = engine.process({4, Side::Buy, OrderType::Limit, to_price(151.50), 60, 0,
                                           OrderStatus::New, now(), 200});
    std::cout << "Trades: " << result2.trades.size() << "\n";
    for (const Trade& t : result2.trades) {
        std::cout << "  buy#" << t.buy_order_id << " <-> sell#" << t.sell_order_id
                  << " at $" << from_price(t.price) << " for " << t.quantity << " units\n";
    }
    std::cout << "Status: ";
    switch (result2.status) {
        case OrderStatus::Filled: std::cout << "Filled\n"; break;
        case OrderStatus::PartiallyFilled: std::cout << "PartiallyFilled\n"; break;
        case OrderStatus::New: std::cout << "New\n"; break;
        default: std::cout << "Other\n"; break;
    }
    std::cout << "Book:\n";
    engine.get_book().print();
    
    return 0;
}