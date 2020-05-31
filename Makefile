all:
	g++ test.cc sampler.cc -std=c++14 -pthread -Ixbyak -O3 -o test
arm:
	g++ test_arm.cc sampler.cc -std=c++14 -pthread -Ixbyak_aarch64 -O3 -o test
