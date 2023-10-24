
all: ptrack.so

ptrack.so: src/ptrack.c Makefile
	$(CC) -shared -fPIC -g -o $@ $< -ldl -lc

clean:
	rm ptrack.so

.PHONY: clean