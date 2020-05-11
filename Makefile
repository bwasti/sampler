all:
	g++ test.cc sampler.cc -std=c++14 -pthread -Ixbyak -o test
