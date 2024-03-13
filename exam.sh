#!/bin/bash

mkdir ./test

mv ./code ./test

sed '' 14.c

a=0
while [$a -lt 16]
do
	gcc -c ./test/code/$a.c
	a=$[$a+1]
done

gcc -o ./test/code/*.o ./test/hello

./test/hello 2> ./test/err.txt

mv ./test/err.txt ./err.txt

chmod rw-r-xr-x err.txt


