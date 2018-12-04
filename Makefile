all:
	clang++ -std=c++14 llvm.cc -O3 -L/home/marijnfs/dev/yagll -lpthread  `llvm-config --cxxflags --ldflags --libs core mcjit native` -ldl -ltinfo -lz -lyagll -ollvm
yopl:
	clang++ -std=c++14 -O3 -L/home/marijnfs/dev/yagll -L/usr/lib/x86_64-linux-gnu  yopl.cc -lre2 -lyagll -oyopl
