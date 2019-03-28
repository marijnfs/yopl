all:
	clang++ -std=c++17 -g  -L/home/marijnfs/dev/yagll -L/usr/lib/x86_64-linux-gnu  yopl.cc `llvm-config --cxxflags --ldflags --libs core mcjit native` -lre2 -lyagll -oyopl -lstdc++fs
llvm:
	clang++ -std=c++17 llvm.cc -O3 -L/home/marijnfs/dev/yagll -lpthread  `llvm-config --cxxflags --ldflags --libs core mcjit native` -ldl -ltinfo -lz -lyagll -ollvm
