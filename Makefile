CC     = gcc
CFLAGS = -Wall -Wextra -g
TESTS  = tests/test_week1 tests/test_week1_persist

all: libfs.a $(TESTS)

libfs.a: fs.o disk.o
	ar rcs libfs.a fs.o disk.o

$(TESTS): %: %.c tests/check.h libfs.a
	$(CC) $(CFLAGS) $< libfs.a -o $@

test: all
	./tests/test_week1
	./tests/test_week1_persist write
	./tests/test_week1_persist read

clean:
	rm -f *.o libfs.a $(TESTS) *.disk

.PHONY: all test clean