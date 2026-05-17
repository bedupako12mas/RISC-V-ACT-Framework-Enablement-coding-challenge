CC     = gcc
CFLAGS = -Wall -Wextra

prog: main.c
	$(CC) $(CFLAGS) main.c -o prog

clean:
	rm -f prog
