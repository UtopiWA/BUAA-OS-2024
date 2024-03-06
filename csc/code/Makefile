.PHONY: clean

INCLUDES = -I ../include

fibo.o: fibo.c ../include/fibo.h
	gcc -c fibo.o fibo.c

main.o: main.c
	gcc -c main.o main.c

clean: 
	rm -f *.o
