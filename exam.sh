#!/bin/bash

mkdir test

cp -r ./code ./test

sed '' ./code/14.c

a=0
while [ $a -lt 16 ]
do
#	gcc -c ./test/code/$a.c > ./test/code/$a.o
	gcc -c ./test/code/$a.c
	mv ./$a.o ./test/code/
	a=$[$a+1]
done
#gcc -c ./test/code/*.c > ./test/code/*.o

gcc ./test/code/*.o -o ./test/hello

./test/hello 2>> ./test/err.txt

mv ./test/err.txt ./err.txt

chmod 655 err.txt

#$1=1
#$2=1
#n=$1+$2
#sed -n '$np' >&2
