
gcc = g++ -std=c++17 --pedantic -Wall

all:	clean compiler

compiler: 
	bison -d -o parser.c parser.y
	flex -o lexer.c lexer.l
	$(gcc) lexer.c parser.c symbols.cpp compiler.cpp -o kompilator

clean: 
	rm -f lexer.c lexer.o parser.c parser.h parser.o kompilator