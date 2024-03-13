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

chmod 655 ./err.txt


if [ $# -eq 0  ]
	n=2
fi
if [ $# -eq 1 ] 
	$n=$[$1+1]
fi
if [ $# -eq 2 ]
	$n=$[$1+$2]
fi

sed -n '$np' err.txt >&2
