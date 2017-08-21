CFLAGS=-Wall -Wextra -g  -Wsign-compare -Wfloat-equal -Wformat-security -pipe -std=gnu99 -pedantic
LDFLAGS=-lip4tc
EXAMPLES=final

all: $(EXAMPLES)


final: src/final/main.o src/final/fw.o src/final/namespace.o src/final/nl.o
	$(CC) $(LDFLAGS) -o $@ $^

src/final/%.o: src/final/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(EXAMPLES) *.o src/*.o src/final/*.o
