#include <iostream>
#include "types.hpp"
#include "matching_engine.hpp"

int main() {
    MatchingEngine engine;
    
    // setting up the book with some resting sell orders (limit)
    engine.process({1, Side::Sell, OrderType::Limit, to_price(151.00), 30, 0,
                    OrderStatus::New, now(), 100});
    engine.process({2, Side::Sell, OrderType::Limit, to_price(151.50), 60, 0,
                    OrderStatus::New, now(), 100});
    
    std::cout << "Book after adding two sell orders:\n";
    engine.get_book().print();
    
    // sending in a buy order that should match
    std::cout << "\n--- Sending buy 50 @ $151.20 ---\n";
    MatchResult result = engine.process({3, Side::Buy, OrderType::Limit, to_price(151.20), 50, 0,
                                         OrderStatus::New, now(), 200});
    
    // print what happened
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

    // test 3: sell-side matching
    std::cout << "\n--- Test 3: sell hits the existing bid ---\n";
    std::cout << "Book before:\n";
    engine.get_book().print();

    // we have a resting bid at $151.20 with 20 units from earlier.
    // send a sell at $151.10 for 10 units. should match against the bid.
    // (sell willing to sell at $151.10 or higher; bid willing to pay $151.20. they cross.)
    MatchResult result3 = engine.process({5, Side::Sell, OrderType::Limit, to_price(151.10), 10, 0,
                                           OrderStatus::New, now(), 300});

    std::cout << "Trades: " << result3.trades.size() << "\n";
    for (const Trade& t : result3.trades) {
        std::cout << "  buy#" << t.buy_order_id << " <-> sell#" << t.sell_order_id
                  << " at $" << from_price(t.price) << " for " << t.quantity << " units\n";
    }

    std::cout << "Status: ";
    switch (result3.status) {
        case OrderStatus::Filled: std::cout << "Filled\n"; break;
        case OrderStatus::PartiallyFilled: std::cout << "PartiallyFilled\n"; break;
        case OrderStatus::New: std::cout << "New\n"; break;
        default: std::cout << "Other\n"; break;
    }

    std::cout << "Book after:\n";
    engine.get_book().print();


    // test 4: order that doesn't match at all
    std::cout << "\n --- Test 4: order that doesn't match at all\n";
    std::cout << "Book before:\n";
    engine.get_book().print();

    // we have a sell at $200 for 100 units - which is above any bid therefore nothing to match against
    // should be trades: 0, status: new, book: should have a new ask at $200 for 100 units, 
    // and the existing bid at $151.20 for 10 units 

    MatchResult result4 = engine.process({6, Side::Sell, OrderType::Limit, to_price(200), 100, 0,
                                           OrderStatus::New, now(), 400});
    
    std::cout << "Trades: " << result4.trades.size() << "\n";
    for (const Trade& t : result4.trades) {
        std::cout << "  buy#" << t.buy_order_id << " <-> sell#" << t.sell_order_id
                  << " at $" << from_price(t.price) << " for " << t.quantity << " units\n";
    }
    
    
    std::cout << "Status: ";
    switch (result4.status) {
        case OrderStatus::Filled: std::cout << "Filled\n"; break;
        case OrderStatus::PartiallyFilled: std::cout << "PartiallyFilled\n"; break;
        case OrderStatus::New: std::cout << "New\n"; break;
        default: std::cout << "Other\n"; break;
    }

    std::cout << "Book after:\n";
    engine.get_book().print();


    // test 5: FIFO at the same price level (each entry in bids_ or asks_ has its own independent queue)
    // when multiple orders rest at the same price, an incoming order 
    // should consume them in the order they arrived

    std::cout << "\n --- Test 5: FIFO at the same price level\n";
    std::cout << "Book before:\n";
    engine.get_book().print();

    // Buy #7
    engine.process({7, Side::Buy, OrderType::Limit, to_price(100), 30, 0,
    OrderStatus::New, now(), 500});
    
    // Buy #8
    engine.process({8, Side::Buy, OrderType::Limit, to_price(100), 30, 0,
    OrderStatus::New, now(), 600});

    // Buy #9
    engine.process({9, Side::Buy, OrderType::Limit, to_price(100), 40, 0,
    OrderStatus::New, now(), 700});

    // Result5 stores the incoming sell #10 at $100 for 80 units
    MatchResult result5 = engine.process({10, Side::Sell, OrderType::Limit, to_price(100), 80, 0,
    OrderStatus::New, now(), 800});

    std::cout << "Trades: " << result5.trades.size() << "\n";
    for (const Trade& t : result5.trades) {
        std::cout << "  buy#" << t.buy_order_id << " <-> sell#" << t.sell_order_id
                  << " at $" << from_price(t.price) << " for " << t.quantity << " units\n";
    }

    std::cout << "Status: ";
    switch (result5.status) {
        case OrderStatus::Filled: std::cout <<"Filled\n"; break;
        case OrderStatus::PartiallyFilled: std::cout << "PartiallyFilled\n"; break;
        case OrderStatus::New: std::cout << "New\n"; break;
        default: std::cout << "Other\n"; break;
    }
    
    std::cout <<"Book after:\n";
    engine.get_book().print();

    // Market Order Test (leftover vanishes)
    std:: cout << "\n --- Test 6: Market Order Test (leftover vanishes)\n";
    std:: cout << "Book before:\n";
    engine.get_book().print();

    // Buy #11
    engine.process({11, Side::Buy, OrderType::Market, to_price(450), 40, 0,
    OrderStatus::New, now(), 800});
    
    // Buy #13
    engine.process({12, Side::Buy, OrderType::Market, to_price(450), 40, 0,
    OrderStatus::New, now(), 800});

    // Buy #13
    engine.process({13, Side::Buy, OrderType::Market, to_price(450), 40, 0,
    OrderStatus::New, now(), 800});

    // Result6 market buy for 80 units
    MatchResult result6 = engine.process({14, Side::Sell, OrderType::Market, to_price(420), 80, 0,
    OrderStatus::New, now(), 800});

    std::cout << "Trades: " << result6.trades.size() << "\n";
    for (const Trade& t : result6.trades) {
        std::cout << "  buy#" << t.buy_order_id << " <-> sell#" << t.sell_order_id
                  << " at $" << from_price(t.price) << " for " << t.quantity << " units\n";
    }

    std::cout << "Status: ";
    switch (result6.status) {
        case OrderStatus::Filled: std::cout <<"Filled\n"; break;
        case OrderStatus::PartiallyFilled: std::cout << "PartiallyFilled\n"; break;
        case OrderStatus::New: std::cout << "New\n"; break;
        default: std::cout << "Other\n"; break;
    }

    std::cout <<"Book after:\n";
    engine.get_book().print();


    // Market Order Test (walks multiple levels)
    std:: cout << "\n --- Test 7: Market Order Test (Walks multiple levels) \n";
    std:: cout << "Book before:\n";
    engine.get_book().print();

    // Sell 15
    engine.process({15, Side::Sell, OrderType::Limit, to_price(250), 20, 0,
    OrderStatus::New, now(), 900});

    // Sell 16
    engine.process({16, Side::Sell, OrderType::Limit, to_price(275), 30, 0,
    OrderStatus::New, now(), 905});

    // Sell 17
    engine.process({17, Side::Sell, OrderType::Limit, to_price(300), 50, 0,
    OrderStatus::New, now(), 910});

    // Result6 stores the 
    MatchResult result7 = engine.process({18, Side::Buy, OrderType::Market, to_price(0), 80, 0,
    OrderStatus::New, now(), 800});

    std::cout << "Trades: " << result7.trades.size() << "\n";
    for (const Trade& t : result7.trades) {
        std::cout << "  buy#" << t.buy_order_id << " <-> sell#" << t.sell_order_id
                  << " at $" << from_price(t.price) << " for " << t.quantity << " units\n";
    }

    std::cout << "Status: ";
    switch (result7.status) {
        case OrderStatus::Filled: std::cout <<"Filled\n"; break;
        case OrderStatus::PartiallyFilled: std::cout << "PartiallyFilled\n"; break;
        case OrderStatus::New: std::cout << "New\n"; break;
        default: std::cout << "Other\n"; break;
    }

    std::cout <<"Book after:\n";
    engine.get_book().print();


    
    return 0;
}