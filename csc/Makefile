.PHONY: clean

fibo:
	make -C ./code fibo.o
	make -C ./code main.o
	gcc -o fibo ./code/fibo.o ./code/main.o

clean:
	make -C ./code clean
