#include <iostream>
#include "types.hpp"
#include "order_book.hpp"


int main() {

    OrderBook book;

    // add some buy orders
    book.add({1, Side::Buy, OrderType::Limit, to_price(150.50), 100, 0,
    OrderStatus::New, now(), 42});

    book.add({2, Side::Buy, OrderType::Limit, to_price(150.50), 50, 0,
              OrderStatus::New, now(), 50});
    book.add({3, Side::Buy, OrderType::Limit, to_price(150.25), 75, 0,
              OrderStatus::New, now(), 77});
    
    // add some sell orders
    book.add({4, Side::Sell, OrderType::Limit, to_price(151.00), 30, 0,
              OrderStatus::New, now(), 21});
    book.add({5, Side::Sell, OrderType::Limit, to_price(151.50), 60, 0,
              OrderStatus::New, now(), 15});
    
    std::cout << "Initial book state:\n\n";
    book.print();
    
    // cancel one order
    auto cancelled = book.cancel(2);
    if (cancelled.has_value()) { // checks if the optional containes an order (true) or is empty (false)
        std::cout << "\nCancelled order #" << cancelled->id 
                  << " for " << cancelled->quantity << " units\n\n";
    }
    
    std::cout << "Book state after cancel:\n\n";
    book.print();
    
    
    return 0;
}