#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MIN(X,Y) ((X) < (Y)? (X) : (Y))
typedef uint8_t u8;

//-- Options ----------------
struct range { size_t start, end; };

size_t option_columns      = 16;
size_t option_groupsize    = 8;
bool option_use_formatting = true;
struct range option_range  = { 0, -1 };

char *formatting_zero      = "38;5;238";
char *formatting_all       = "38;5;167";
char *formatting_low       = "38;5;150";
char *formatting_high      = "38;5;141";
char *formatting_printable = "";

//-- Hexdump impl -----------
const char *format_of(int v) {
  return v == 0x00?  formatting_zero
       : v == 0xFF?  formatting_all
       : v <  0x20?  formatting_low
       : v >= 0x7F?  formatting_high
       :             formatting_printable;
}

void hexdump(FILE *f) {
  u8 buf[BUFSIZ];
  u8 last_line[option_columns];

  bool first_line = true, printed_asterisk = false;

  // Seek to start; fall back to a consuming loop for non-seekable files
  if (fseek(f, option_range.start, SEEK_SET) < 0) {
    size_t remaining = option_range.start;
    while (remaining != 0 && !feof(f) && !ferror(f)) {
      size_t n = fread(buf, 1, MIN(remaining, BUFSIZ), f);
      remaining -= n;
      if (n == 0) break;
    }
    if (ferror(f)) err(1, NULL);
  }

  size_t count = option_range.end - option_range.start;
  size_t read = 0;
  while (read != count && !feof(f) && !ferror(f)) {
    size_t n = fread(buf, 1, MIN(count - read, BUFSIZ), f);

    for (size_t i = 0; i < n; i += option_columns) {
      u8 *p = &buf[i];

      // Contract repeated identical lines
      if (!first_line && memcmp(p, last_line, option_columns) == 0) {
        if (!printed_asterisk) {
          printf("%8s\n", "*");
          printed_asterisk = true;
        }
        continue;
      }
      printed_asterisk = false;

      // Offset
      size_t offset = option_range.start + read + i;
      printf("%5zx%03zx", offset >> 12, offset & 0xFFF);

      // Print hex area
      const char *prev_fmt = NULL;
      for (size_t j = 0; j < option_columns; j++) {
        if (option_groupsize != 0 && j % option_groupsize == 0) printf(" ");
        if (i + j < n) {
          const char *fmt = format_of(p[j]);
          if (prev_fmt != fmt && option_use_formatting) printf("\x1B[%sm", fmt);
          printf(" %02x", p[j]);
          prev_fmt = fmt;
        } else {
          printf("   ");
        }
      }
      putchar(' ');

      // Print char area
      for (size_t j = 0; j < option_columns; j++) {
        if (option_groupsize != 0 && j % option_groupsize == 0) printf(" ");
        if (i + j < n) {
          const char *fmt = format_of(p[j]);
          if (prev_fmt != fmt && option_use_formatting) printf("\x1B[%sm", fmt);
          putchar(isprint(p[j])? p[j] : '.');
          prev_fmt = fmt;
        } else {
          putchar(' ');
        }
      }
      printf("%s\n", option_use_formatting? "\x1B[m" : "");

      memcpy(last_line, p, MIN(n - i, option_columns));
      first_line = false;
    }

    read += n;
  }

  if (ferror(f)) err(1, NULL);
}

//-- Entry point ------------
/** Parses a range "start-end" (both ends optional) or "start+size" (neither
 *  optional) into a `struct range` instance. */
struct range parse_range(const char *str) {
  struct range res = { 0, -1 };
  const char *first = str, *delim = str + strcspn(str, "+-"), *second = delim + 1;
  if (*delim == '\0') errx(1, "no delimiter in range %s", str);

  char *end;
  if (first != delim) {
    res.start = strtol(first, &end, 0);
    if (!isdigit(*first) || end != delim) errx(1, "invalid start value in range %s", str);
  }
  if (*second != '\0') {
    res.end = strtol(second, &end, 0);
    if (!isdigit(*second) || *end != '\0') errx(1, "invalid end/size value in range %s", str);
  }

  if (*delim == '+') {
    if (first == delim) errx(1, "start unspecified in range %s", str);
    if (*second == '\0') errx(1, "size unspecified in range %s", str);
    res.end += res.start;
  }

  if (res.end < res.start) errx(1, "end was less than start in range %s", str);
  return res;
}

int main(int argc, char *argv[]) {
  // Parse options
  int opt;
  while (opt = getopt(argc, argv, "g:pr:w:"), opt != -1) {
    switch (opt) {
      case 'g': option_groupsize = atol(optarg); break;
      case 'p': option_use_formatting = false; break;
      case 'r': option_range = parse_range(optarg); break;
      case 'w': option_columns = atol(optarg); break;
      default:
        fprintf(stderr, "usage: hexd [-p] [-g groupsize] [-r range] [-w width]\n");
        return 1;
    }
  }
  argc -= optind;
  argv += optind;

  // Parse HEXD_COLORS
  char *colors_var = getenv("HEXD_COLORS");
  if (colors_var != NULL) {
    colors_var = strdup(colors_var);
    if (colors_var == NULL) errx(1, "strdup");

    for (char *p = colors_var, *token; token = strtok(p, " "), token != NULL; p = NULL) {
      char *key = token, *value = strchr(token, '=');
      if (value == NULL) warnx("no '=' found in HEXD_COLORS property '%s'", p);
      *value++ = '\0';

      if      (strcmp(key, "zero")      == 0) formatting_zero      = value;
      else if (strcmp(key, "low")       == 0) formatting_low       = value;
      else if (strcmp(key, "printable") == 0) formatting_printable = value;
      else if (strcmp(key, "high")      == 0) formatting_high      = value;
      else if (strcmp(key, "all")       == 0) formatting_all       = value;
      else warnx("unknown HEXD_COLORS property '%s'", key);
    }
  }

  // Hexdump files
  if (argc == 0) {
    hexdump(stdin);
  } else {
    for (size_t i = 0; i < argc; i++) {
      FILE *f = fopen(argv[i], "r");
      if (f == NULL) warn("%s", argv[i]);

      if (argc > 1) {
        printf("%s====> %s%s%s <====\n", i > 0? "\n" : "",
                                         option_use_formatting? "\x1B[1m" : "",
                                         argv[i],
                                         option_use_formatting? "\x1B[m" : "");
      }

      hexdump(f);
      fclose(f);
    }
  }

  free(colors_var);
  return 0;
}
