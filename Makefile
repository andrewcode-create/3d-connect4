all:
	clang++ -std=c++23 -O2 -g main.cpp -o connect4.out -march=native
clean:
	rm -f connect4.out