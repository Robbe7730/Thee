all:
	clang++ -g -O3 src/thee.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o target/waterkoker

debug:
	clang++ -g -O3 src/thee.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o target/waterkoker -DDEBUG
