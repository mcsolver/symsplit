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
test:
	./bin/run.o min_max ./data/tests/general/pattern ./data/tests/general/target -l -q -t 100

