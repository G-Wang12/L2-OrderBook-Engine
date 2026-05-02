#include <iostream>
#include <string>

#include "order_book.hpp"

int main()
{
    std::cout << "=========================================" << std::endl;
    std::cout << "Low Latency Prediction Market Engine" << std::endl;
    std::cout << "Version 0.1.0 - C++20 Optimized Build" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Startup successful. Ready for market data ingestion," << std::endl;
    std::cout << "order matching, and low-latency execution." << std::endl;
    std::cout << std::endl;
    std::cout << "Built with:" << std::endl;
    std::cout << "  - SIMD JSON parsing (zero-copy)" << std::endl;
    std::cout << "  - Boost.Asio for networking" << std::endl;
    std::cout << "  - Native optimizations (-O3 -march=native)" << std::endl;
    std::cout << std::endl;
    std::cout << "Engine initialized at " << __DATE__ << " " << __TIME__ << std::endl;

    return 0;
}
