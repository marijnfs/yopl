all:
	ninja -C build
bla:
	clang++ -g `llvm-config --cxxflags --ldflags --libs core mcjit native`  -std=c++17 -L/home/marijnfs/dev/yagll -L/usr/lib/x86_64-linux-gnu  yopl.cc -lyagll -lstdc++fs -oyopl
llvm:
	clang++ -std=c++17 llvm.cc -O3 -L/home/marijnfs/dev/yagll -lpthread  `llvm-config --cxxflags --ldflags --libs core mcjit native` -ldl -ltinfo -lz -lyagll -ollvm
