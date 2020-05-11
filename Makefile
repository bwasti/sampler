all:
	g++ test.cc -std=c++14 -pthread -Ixbyak && sudo ./a.out
