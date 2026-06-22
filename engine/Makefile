all: engine

engine: src/main.cpp src/order_book.cpp
	g++ -std=c++17 -Wall -Wextra -O2 src/main.cpp src/order_book.cpp -o engine

clean:
	rm -f engine