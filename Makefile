CXXFLAGS = -O3 -ffast-math -pedantic -Wall -std=c++17 -pthread -g

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
# macOS: Apple clang + Homebrew deps. Resolved via `brew --prefix` so it works on
# Apple Silicon (/opt/homebrew) and Intel (/usr/local) and survives version bumps.
CXX           = /usr/bin/clang++
BREW_PREFIX  := $(shell brew --prefix 2>/dev/null)
ARGP_PREFIX   = $(BREW_PREFIX)/opt/argp-standalone
CATCH2_PREFIX = $(BREW_PREFIX)/opt/catch2
ARGP_FLAGS    = -I$(ARGP_PREFIX)/include -L$(ARGP_PREFIX)/lib -largp
CATCH2_FLAGS  = -I$(CATCH2_PREFIX)/include -L$(CATCH2_PREFIX)/lib
CHECK_DEPS    = test -d "$(ARGP_PREFIX)" || { echo "argp-standalone not found. Run: brew install argp-standalone"; exit 1; }
CHECK_CATCH2  = test -d "$(CATCH2_PREFIX)" || { echo "catch2 not found. Run: brew install catch2"; exit 1; }
else
# Linux: g++; argp is part of glibc, Catch2 from the system package manager.
CXX           = g++
ARGP_FLAGS    =
CATCH2_FLAGS  =
CHECK_DEPS    = true
CHECK_CATCH2  = true
endif

all: build
build:
	@$(CHECK_DEPS)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) *.cpp -o bin/run.o $(ARGP_FLAGS)
# Kept for backwards compatibility; both now auto-detect the OS.
macbuild linuxbuild: build
run:
	./bin/run.o min_max ./data/tests/general/pattern ./data/tests/general/target -l -q -t 100

test: test-lad test-dimacs test-unit
	@echo "All tests passed."

test-unit:
	@$(CHECK_CATCH2)
	@mkdir -p bin
	@echo "Building unit tests..."
	@$(CXX) -O0 -std=c++17 -pthread \
		graph.cpp solver.cpp tests/test_solver.cpp \
		$(CATCH2_FLAGS) -lCatch2Main -lCatch2 \
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

