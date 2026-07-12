PYBIND_INCLUDES := $(shell python3 -m pybind11 --includes)
PY_SUFFIX := $(shell python3-config --extension-suffix)

all: engine python-module

engine: src/main.cpp src/order_book.cpp src/matching_engine.cpp
	g++ -std=c++17 -Wall -Wextra -O2 src/main.cpp src/order_book.cpp src/matching_engine.cpp -o engine

python-module: src/bindings.cpp src/order_book.cpp src/matching_engine.cpp
	g++ -std=c++17 -Wall -Wextra -O2 -fPIC -shared -undefined dynamic_lookup $(PYBIND_INCLUDES) src/bindings.cpp src/order_book.cpp src/matching_engine.cpp -o trading_engine$(PY_SUFFIX)
clean:
	rm -f engine trading_engine*.so
	