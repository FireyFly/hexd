 Utilities
===========

Various more-or-less useful utilities and tools.


## hexd
  hexd is a simple `hexdump`-alike with pretty colours to make it easy to
  discern structure at a glance.  Colours are configurable via the
  `HEXD_COLORS` environment variable.  Limited support for command-line flags;
  try `-h` for more information.

  ![hexd screenshot](https://github.com/FireyFly/utilities/raw/master/meta/hexd.png)


## plaintext
  A simple filter that removes all CSI control sequences from stdin and copies
  to stdout.  This includes SGR formatting such as colours, as well as other
  control sequences for cursor movement etc.
