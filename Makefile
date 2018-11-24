all:
	clang++ -std=c++14 llvm.cc -O3 -L/home/marijnfs/dev/yagll -lpthread  `llvm-config --cxxflags --ldflags --libs core mcjit native` -ldl -ltinfo -lz -lyagll
yopl:
	clang++ -std=c++14 -O3 -L/usr/lib/x86_64-linux-gnu parsegraph.cc  parser.cc  ruleset.cc  yopl.cc -lre2
