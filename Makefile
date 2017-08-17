CFLAGS=-Wall -Wextra -g -std=c99 -pipe

EXAMPLES=pair

all: $(EXAMPLES)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(EXAMPLES)
