CFLAGS=-Wall -Wextra -g -std=c99 -pipe

EXAMPLES=pair link_address refactor namespace

all: $(EXAMPLES)

%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

%: %.o
	$(CC) -o $@ $<

clean:
	rm -rf $(EXAMPLES)
