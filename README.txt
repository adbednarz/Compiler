Adam Bednarz - 250297

Kompilacja:
----------------------------------------
make

./kompilator <path_file_test> <output file>

Pliki:
----------------------------------------
README.txt
Makefile
compiler.h
compiler.cpp
symbols.h
symbols.cpp
lexer.l
parser.y

Obsługa błędów:
----------------------------------------
W przypadku błędów w kodzie źródłowym wypisywany jest odpowiedni komunikat, ale także jest generowany pusty plik wyjściowy

Narzędzia:
----------------------------------------
bison (GNU Bison) 3.5.1
flex 2.6.4
g++ 9.3.0

