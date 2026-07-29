CC ?= cc
CFLAGS ?= -Wall -Wextra -g

sicc: src/sicc.c
	$(CC) $(CFLAGS) -o $@ $< -lm

.PHONY: test clean

test: sicc
	tests/run.sh

clean:
	rm -f sicc
	rm -rf tests/out
