CFLAGS += -std=c11 -g

BINARIES=hexd

.PHONY: all
all: $(BINARIES)

.PHONY: clean
clean:
	rm -f $(BINARIES)

hexd: hexd.c
	$(CC) $(CFLAGS) -o $@ $^
