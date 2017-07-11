all:

orig:
	g++ -c MyClass.cc -o MyClass.o

MyMain_cpp:
	g++ -c MyMain_c++.c -o MyMain_c++.o
	g++ MyMain_c++.o MyClass.o -o MyMain_cpp

wrapper:
	g++ -c MyWrapper.cc -o MyWrapper.o

MyMain_c:
	gcc -c MyMain_c.cc -o MyMain_c.o
	g++ MyMain_c.o MyWrapper.o MyClass.o -o MyMain_c
