all:
	clang++ -std=c++23 -O3 -o connect4.out -march=native main.cpp
clean:
	rm -f connect4.out