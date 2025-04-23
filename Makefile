OPTIONS=--std=c99 -Wall -g

flashsim: main.o disk.o flash.o
	gcc main.o disk.o flash.o -o flashsim

main.o: main.c disk.h flash.h
	gcc ${OPTIONS} -c main.c -o main.o

disk.o: disk.c disk.h
	gcc ${OPTIONS} -c disk.c -o disk.o

flash.o: flash.c flash.h
	gcc ${OPTIONS} -c flash.c -o flash.o

clean:
	rm -f flashsim *.o

