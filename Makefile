all:
	g++ src/thee.cpp -o target/waterkoker

debug:
	g++ src/thee.cpp -o target/waterkoker -DDEBUG
