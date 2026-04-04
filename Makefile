all:
	g++ -O3 -ffast-math -pedantic -Wall -std=c++17 *.cpp -pthread -o bin/run.o
build:
	g++ -O3 -ffast-math -pedantic -Wall -std=c++17 *.cpp -pthread -o bin/run.o
linuxbuild:
	g++ -O3 -ffast-math -pedantic -Wall -std=c++17 *.cpp -g -pthread -o bin/run.o
macbuild:
	/usr/bin/clang++ -O3 -ffast-math -pedantic -Wall -std=c++17 *.cpp -pthread -g -o bin/run.o -I/opt/homebrew/Cellar/boost/1.86.0_2/include/ -I/opt/homebrew/Cellar/argp-standalone/1.3/include -L/opt/homebrew/Cellar/argp-standalone/1.3/lib/ -largp
run:
	./bin/run.o min_max ./data/tests/general/pattern ./data/tests/general/target -l -q -t 100
CATCH2_INC = /opt/homebrew/Cellar/catch2/3.13.0/include
CATCH2_LIB = /opt/homebrew/Cellar/catch2/3.13.0/lib
ARGP_INC   = /opt/homebrew/Cellar/argp-standalone/1.3/include
ARGP_LIB   = /opt/homebrew/Cellar/argp-standalone/1.3/lib

test: test-lad test-dimacs test-unit
	@echo "All tests passed."

test-unit:
	@echo "Building unit tests..."
	@/usr/bin/clang++ -O0 -std=c++17 -pthread \
		-I$(CATCH2_INC) \
		-L$(CATCH2_LIB) -lCatch2Main -lCatch2 \
		graph.cpp solver.cpp tests/test_solver.cpp \
		-o bin/test.o
	@echo "Running unit tests..."
	@./bin/test.o

test-lad:
	@echo "Running LAD test..."
	@actual=$$(./bin/run.o min_max ./data/tests/general/pattern ./data/tests/general/target -l -q -t 100 | head -1); \
	expected="7, 1, "; \
	if echo "$$actual" | grep -q "^7, 1,"; then \
		echo "  LAD test passed (MCS size: 7)"; \
	else \
		echo "  LAD test FAILED. Got: $$actual"; exit 1; \
	fi

test-dimacs:
	@echo "Running DIMACS test..."
	@actual=$$(./bin/run.o min_max ./data/tests/dimacs/pattern ./data/tests/dimacs/target -d -q -t 100 | head -1); \
	if echo "$$actual" | grep -q "^10, 1,"; then \
		echo "  DIMACS test passed (MCS size: 10)"; \
	else \
		echo "  DIMACS test FAILED. Got: $$actual"; exit 1; \
	fi

