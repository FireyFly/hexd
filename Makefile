CFLAGS += -std=c11

BINARIES=bin/hexd

.PHONY: all
all: $(BINARIES)

.PHONY: clean
clean:
	rm -f $(BINARIES)

bin/hexd: hexd/hexd.c
	$(CC) $(CFLAGS) -o $@ $^
