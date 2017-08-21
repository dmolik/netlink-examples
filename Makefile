CFLAGS=-Wall -Wextra -g  -Wsign-compare -Wfloat-equal -Wformat-security -std=gnu99 -pipe

EXAMPLES=pair link_address refactor namespace pair_ns ns_addr ns_gw masquerade forward

all: $(EXAMPLES)

forward: src/forward.o
	$(CC) -lip4tc -o $@ $<

masquerade: src/masquerade.o
	$(CC) -lip4tc -o $@ $<

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

%: src/%.o
	$(CC) -o $@ $<

clean:
	rm -rf $(EXAMPLES) src/*.o
