
rm -r ./bin/
mkdir ./bin/
clang -ObjC -g ./src/test.c \
	-o bin/test \


./bin/test
