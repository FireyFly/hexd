CFLAGS += -Wall -std=c11

ifeq ($(DEBUG),1)
  CFLAGS += -g -DDEBUG
endif

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
SHAREDIR=$(PREFIX)/share

.PHONY: all
all: hexd hexd.1.gz

.PHONY: clean
clean:
	rm -f hexd hexd.1.gz

.PHONY: install
install:
	install -D hexd $(DESTDIR)$(BINDIR)/hexd
	install -D hexd.1.gz $(DESTDIR)$(SHAREDIR)/man/man1/hexd.1.gz

hexd: hexd.c
	$(CC) $(CFLAGS) -o $@ $^

hexd.1.gz: hexd.1
	gzip -k hexd.1
