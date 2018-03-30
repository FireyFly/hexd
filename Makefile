CFLAGS += -Wall -std=c11

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

.PHONY: all
all: hexd

.PHONY: clean
clean:
	rm -f hexd

.PHONY: install
install: hexd
	install -D hexd $(DESTDIR)$(BINDIR)/hexd
	install -D hexd.1 $(DESTDIR)$(MANDIR)/man1/hexd.1
