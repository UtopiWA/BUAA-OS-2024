#!/bin/bash

mkdir ./test

cp -r ./code ./test

sed '' ./code/14.c

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

$1=1
$2=1
n=$1+$2
sed -n '$np' >&2
