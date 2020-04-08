# Author: Jordan K Bartos
# Date: February 4, 2020
# file: makefile
# Description: this is the makefile instructions for compiling the ftserver program


ftserver: ftserver.o
	gcc -g -Wall -o ftserver ftserver.o

ftserver.o: ftserver.c
	gcc -c -g -Wall ftserver.c

clean:
	rm ftserver.o ftserver 

debug:
	make
	valgrind -v --leak-check=full --show-leak-kinds=all ./ftserver $(port)
