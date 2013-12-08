CFLAGS += -std=c99

.PHONY: all
all: hexd

.PHONY: clean
clean:
	rm *.o hexd

hexd: hexd.o
	$(CC) -o $@ $^
