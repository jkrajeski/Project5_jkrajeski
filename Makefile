CC = gcc
CFLAGS = -Wall -Wextra -g

all: test_week1 test_week1_persist

test_week1: test_week1.c fs.o disk.o check.h fs.h disk.h
	$(CC) $(CFLAGS) test_week1.c fs.o disk.o -o test_week1

test_week1_persist: test_week1_persist.c fs.o disk.o check.h fs.h disk.h
	$(CC) $(CFLAGS) test_week1_persist.c fs.o disk.o -o test_week1_persist

fs.o: fs.c fs.h disk.h
	$(CC) $(CFLAGS) -c fs.c -o fs.o

disk.o: disk.c disk.h
	$(CC) $(CFLAGS) -c disk.c -o disk.o

test: all
	./test_week1
	./test_week1_persist write
	./test_week1_persist read

clean:
	rm -f *.o test_week1 test_week1_persist *.disk