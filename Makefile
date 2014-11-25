CFLAGS += -std=c99

BINARIES=hexd plaintext

.PHONY: all
all: bin $(BINARIES)

.PHONY: clean
clean:
	cd bin; rm -f $(BINARIES)
	rmdir bin

bin:
	mkdir bin

%: src/%.c
	$(CC) $(CFLAGS) -o bin/$@ $^
