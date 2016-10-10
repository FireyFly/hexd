CFLAGS += -std=c11

BINARIES=hexd

.PHONY: all
all: $(BINARIES)

.PHONY: clean
clean:
	cd bin; rm -f $(BINARIES)

%: src/%.c
	$(CC) $(CFLAGS) -o bin/$@ $^
