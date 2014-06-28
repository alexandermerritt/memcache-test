all:	test

test:	test.c
	clang $^ -o $@ -O3 -Wall -Werror -lmemcached -lpthread

clean:
	rm -f test
