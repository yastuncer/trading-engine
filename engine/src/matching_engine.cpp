#include "matching_engine.hpp"
#include <algorithm>


/*
The process() --> the matching system (the brain)
An incoming order arrives, the engine needs to:
    1. try to match it against resting orders on the opposite side
    2. for each match create a trade and update both orders
    3. if the incoming order has remaining quantity and its a limit order, rest it on the book
    4. if the incoming order has remaining quantity and its a market order, drop the rest
return: a list of trades, the final status of the incoming order, and the resting order if
it ended up on the book
*/



MatchResult MatchingEngine::process(const Order& order) {

    MatchResult result;
    Order incoming_order = order; // copy into a local variable to modify it durint matching

    if (incoming_order.side == Side::Buy) { // buy branch --> match against asks (sell orders)
        while (incoming_order.remaining() > 0) { // while the incoming order still has unfilled quantity, keep matching.
            const PriceLevel* best_ask_level = book_.best_ask(); // calling best_ask() on the order book and then returns a ptr to best ask's PriceLevel
            if (best_ask_level == nullptr) break; //checking if there are asks in the book
            if (best_ask_level->price > incoming_order.price) break; // making sure the best ask price is not > incoming price
            const Order& resting = best_ask_level->orders.front(); // get the first order in the deque by returning a ref to first order

            // compute how much fills needed
            Quantity fill_qty = std::min(incoming_order.remaining(), resting.remaining()); // min returns the smaller of the 2 values, 3 possible scenarios min() handles
            // creating the trade
            Trade trade;
            trade.buy_order_id = incoming_order.id;
            trade.sell_order_id = resting.id;
            trade.price = resting.price; // resting order arrived first and committed to its price
            trade.quantity = fill_qty;
            trade.timestamp = now();

            // adding the trade to results
            result.trades.push_back(trade);
            // update the incoming order's filled quantity
            incoming_order.filled_qty += fill_qty;
            // apply the fill to the resting order
            book_.apply_fill(resting.id, fill_qty);
        }
    } else {
        // sell branch --> match against bids (buy orders)
        while(incoming_order.remaining() > 0) {
            const PriceLevel* best_bid_level = book_.best_bid();
            if(best_bid_level == nullptr) break;
            if(best_bid_level->price < incoming_order.price) break;
            const Order& resting = best_bid_level->orders.front();

            // compute how much fills
            Quantity fill_qty = std::min(incoming_order.remaining(), resting.remaining()); // min returns the smaller of the 2 values
            // creating the trade
            Trade trade;
            trade.buy_order_id = resting.id;  // the resting bid is the buyer
            trade.sell_order_id = incoming_order.id; // the incoming sell is the seller
            trade.price = resting.price;
            trade.quantity = fill_qty;
            trade.timestamp = now();

            // adding the trade to results
            result.trades.push_back(trade);
            // update the incoming order's filled quantity
            incoming_order.filled_qty += fill_qty;
            // apply the fill to the resting order
            book_.apply_fill(resting.id, fill_qty);
        }
      
    }
    // rest the leftover (this is only for limit orders with remaining quantity)
    if (incoming_order.remaining() > 0 && incoming_order.type == OrderType::Limit) {
        book_.add(incoming_order); // add order to the book
        result.resting_order = incoming_order; // leftover ends up in the book
    }
    // setting the status based on fill state
    if (incoming_order.is_filled()) {
        result.status = OrderStatus::Filled; // order is completely full
    } else if (incoming_order.filled_qty > 0) {
        result.status = OrderStatus::PartiallyFilled; // order is partially filled
    } else {
       result.status = OrderStatus::New; // no matches happened
    }

    return result;

}



/*
Buy-limit case
create an empty MatchResult called result
make a local copy of incoming_order (so we can modify filled_qty as we match)


pseudocode
incoming order comes
if incoming order is a Buy:
while (incoming order has remaining quantity && there are asks in the book && the incoming order <= the asks price):
    look at the best ask price which will be book_.best_ask()
    look at the first order in that price level's deque (FIFO)
    let resting = that first order

    let fill_qty = min(incoming remaining, resting remaining)
    
    create a Trade with:
        buy_order_id = incoming_id
        sell_order_id = resting.id
        price = resting.price
        quantity = fill_qty
        timestamp = now()
    add the Trade to result.trades
    incoming.filled_qty += fill_qty
    apply_fill(resting.id, fill_qty)
if incoming has remaining quantity:
    book_.add(incoming)
    result.resting_order = incoming

if incoming.is_filled():
    result.status = Filled
else if incoming.filled_qty > 0:
    result.status = PartiallyFilled
else:
    result.status = New

return result
*/

const OrderBook& MatchingEngine::get_book() const {
    return book_;
}