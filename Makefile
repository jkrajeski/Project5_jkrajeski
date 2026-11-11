CC     = gcc
CFLAGS = -Wall -Wextra -g

all: libfs.a

libfs.a: fs.o disk.o
	ar rcs libfs.a fs.o disk.o

clean:
	rm -f *.o libfs.a	