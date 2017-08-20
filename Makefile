CFLAGS=-Wall -Wextra -g  -Wsign-compare -Wfloat-equal -Wformat-security -std=gnu99 -pipe

EXAMPLES=pair link_address refactor namespace pair_ns ns_addr ns_gw masquerade forward

all: $(EXAMPLES)

forward: forward.o
	$(CC) -lip4tc -o $@ $<

masquerade: masquerade.o
	$(CC) -lip4tc -o $@ $<

%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

%: %.o
	$(CC) -o $@ $<

clean:
	rm -rf $(EXAMPLES)
