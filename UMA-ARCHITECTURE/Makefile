# ============================================================
#  UMA — Unambiguous Memory Acknowledgment
#  Makefile — builds demo, tests, and benchmark
# ============================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
OPT      = -O2
INCLUDE  = -I.

.PHONY: all demo tests benchmark clean

all: demo tests benchmark

# Main demo
demo: main.cpp msf/include/msf.hpp
	$(CXX) $(CXXFLAGS) $(OPT) $(INCLUDE) -o uma_demo main.cpp
	@echo "Built: uma_demo"

# Full test suite
tests: tests/test_all.cpp msf/include/msf.hpp
	$(CXX) $(CXXFLAGS) $(OPT) $(INCLUDE) -o uma_tests tests/test_all.cpp
	@echo "Built: uma_tests"

# Benchmark
benchmark: benchmark/benchmark.cpp msf/include/msf.hpp
	$(CXX) $(CXXFLAGS) $(OPT) $(INCLUDE) -o uma_benchmark benchmark/benchmark.cpp
	@echo "Built: uma_benchmark"

# Run all
run: all
	@echo "\n=== Running Demo ==="
	./uma_demo
	@echo "\n=== Running Tests ==="
	./uma_tests
	@echo "\n=== Running Benchmark ==="
	./uma_benchmark

clean:
	rm -f uma_demo uma_tests uma_benchmark
