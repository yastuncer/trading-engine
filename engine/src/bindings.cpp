#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "order_book.hpp"
#include "types.hpp"
#include "matching_engine.hpp"

namespace py = pybind11;

PYBIND11_MODULE(trading_engine, m) {
    // everything I expose

    // Side
    py::enum_<Side>(m, "Side")
        .value("Buy", Side::Buy)
        .value("Sell", Side::Sell)
        .export_values();
    
    // OrderType
    py::enum_<OrderType>(m, "OrderType")
        .value("Limit", OrderType::Limit)
        .value("Market", OrderType::Market)
        .export_values();

    // OrderStatus
    py::enum_<OrderStatus>(m, "OrderStatus")
        .value("New", OrderStatus::New)
        .value("PartiallyFilled", OrderStatus::PartiallyFilled)
        .value("Filled", OrderStatus::Filled)
        .value("Cancelled", OrderStatus::Cancelled)
        .value("Rejected", OrderStatus::Rejected)
        .export_values();


    // Order
    py::class_<Order>(m, "Order")
        .def(py::init<>()) // default constructor
        .def_readwrite("id", &Order::id)
        .def_readwrite("side", &Order::side)
        .def_readwrite("type", &Order::type)
        .def_readwrite("price", &Order::price)
        .def_readwrite("quantity", &Order::quantity)
        .def_readwrite("filled_qty", &Order::filled_qty)
        .def_readwrite("status", &Order::status)
        .def_readwrite("timestamp", &Order::timestamp)
        .def_readwrite("user_id", &Order::user_id)
        .def("remaining", &Order::remaining)
        .def("is_filled", &Order::is_filled);


    // Trade
    py::class_<Trade>(m, "Trade")
        .def(py::init<>())
        .def_readwrite("buy_order_id", &Trade::buy_order_id)
        .def_readwrite("sell_order_id", &Trade::sell_order_id)
        .def_readwrite("price", &Trade::price)
        .def_readwrite("quantity", &Trade::quantity)
        .def_readwrite("timestamp", &Trade::timestamp);


    // PriceLevel
    py::class_<PriceLevel>(m, "PriceLevel")
        .def_readonly("price", &PriceLevel::price)
        .def_readonly("orders", &PriceLevel::orders);


    // OrderBook
    py::class_<OrderBook>(m, "OrderBook")
        .def(py::init<>())
        .def("add", &OrderBook::add)
        .def("cancel", &OrderBook::cancel)
        .def("best_bid", &OrderBook::best_bid)
        .def("best_ask", &OrderBook::best_ask)
        .def("print", &OrderBook::print, py::arg("depth") = 5)
        .def("apply_fill", &OrderBook::apply_fill)
        .def("clear", &OrderBook::clear);

    // MatchResult
    py::class_<MatchResult>(m, "MatchResult")
        .def(py::init<>())
        .def_readwrite("trades", &MatchResult::trades)
        .def_readwrite("status", &MatchResult::status)
        .def_readwrite("resting_order", &MatchResult::resting_order);

    // MatchEngine
    py::class_<MatchingEngine>(m, "MatchingEngine")
        .def(py::init<>())
        .def("process", &MatchingEngine::process)
        .def("get_book", &MatchingEngine::get_book, py::return_value_policy::reference);





    


}