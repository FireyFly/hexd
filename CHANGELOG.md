# Changelog

## 1.1.0
  * default to colours/formatting based on whether output is a TTY
  * add verbose option to show all bytes (and not omit repeated lines)
  * add -h as an option to show usage
  * fix misaligned output in case -w width doesn't divide BUFSIZ
  * fix downcasting issue from `off_t` to a potentially smaller size

## 1.0.0
  initial release
