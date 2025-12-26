
rm -r ./bin/
mkdir ./bin/
clang -ObjC -g ./src/test.c \
	-I ./src/ \
	-o bin/test \


./bin/test
