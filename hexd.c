#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <err.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MIN(X,Y) ((X) < (Y)? (X) : (Y))
typedef uint8_t u8;

//-- Options ----------------
struct offset_range { off_t start, end; };

size_t option_columns      = 16;
size_t option_groupsize    = 8;
bool option_use_formatting = true;
struct offset_range option_range  = { 0, -1 };

const char *formatting_zero      = "38;5;238";
const char *formatting_all       = "38;5;167";
const char *formatting_low       = "38;5;150";
const char *formatting_high      = "38;5;141";
const char *formatting_printable = "";

//-- Hexdump impl -----------
const char *format_of(int v) {
  return v == 0x00?  formatting_zero
       : v == 0xFF?  formatting_all
       : v <  0x20?  formatting_low
       : v >= 0x7F?  formatting_high
       :             formatting_printable;
}

const char *CHAR_AREA_HIGH_LUT[] = {
  "€", ".", "‚", "ƒ", "„", "…", "†", "‡", "ˆ", "‰", "Š", "‹", "Œ", ".", "Ž", ".",
  ".", "‘", "’", "“", "”", "•", "–", "—", "˜", "™", "š", "›", "œ", ".", "ž", "Ÿ",
  ".", "¡", "¢", "£", "¤", "¥", "¦", "§", "¨", "©", "ª", "«", "¬", ".", "®", "¯",
  "°", "±", "²", "³", "´", "µ", "¶", "·", "¸", "¹", "º", "»", "¼", "½", "¾", "¿",
  "À", "Á", "Â", "Ã", "Ä", "Å", "Æ", "Ç", "È", "É", "Ê", "Ë", "Ì", "Í", "Î", "Ï",
  "Ð", "Ñ", "Ò", "Ó", "Ô", "Õ", "Ö", "×", "Ø", "Ù", "Ú", "Û", "Ü", "Ý", "Þ", "ß",
  "à", "á", "â", "ã", "ä", "å", "æ", "ç", "è", "é", "ê", "ë", "ì", "í", "î", "ï",
  "ð", "ñ", "ò", "ó", "ô", "õ", "ö", "÷", "ø", "ù", "ú", "û", "ü", "ý", "þ", "ÿ",
};

void hexdump(FILE *f, const char *filename) {
  u8 line[option_columns];
  u8 prev_line[option_columns];

  bool first_line = true, printed_asterisk = false;

  // Seek to start; fall back to a consuming loop for non-seekable files
  if (fseeko(f, option_range.start, SEEK_SET) < 0) {
    off_t remaining = option_range.start;
    while (remaining != 0 && fgetc(f) != EOF) remaining--;
    if (ferror(f)) err(1, "(while seeking) %s", filename);
  }

  for (off_t offset = option_range.start; offset < option_range.end || option_range.end == -1; offset += option_columns) {
    off_t read = offset - option_range.start;
    size_t n = fread(line, 1, option_columns, f);
    if (n == 0) break;

    // Contract repeated identical lines
    if (!first_line && memcmp(line, prev_line, option_columns) == 0 && n == option_columns) {
      if (!printed_asterisk) {
        printf("%8s\n", "*");
        printed_asterisk = true;
      }
      continue;
    }
    printed_asterisk = false;

    // Offset
    intmax_t offset = option_range.start + read;
    printf("%5jx%03jx", offset >> 12, offset & 0xFFF);

    // Print hex area
    const char *prev_fmt = NULL;
    for (size_t j = 0; j < option_columns; j++) {
      if (option_groupsize != 0 && j % option_groupsize == 0) printf(" ");
      if (j < n) {
        const char *fmt = format_of(line[j]);
        if (prev_fmt != fmt && option_use_formatting) printf("\x1B[%sm", fmt);
        printf(" %02x", line[j]);
        prev_fmt = fmt;
      } else {
        printf("   ");
      }
    }
    putchar(' ');

    // Print char area
    for (size_t j = 0; j < option_columns; j++) {
      if (option_groupsize != 0 && j % option_groupsize == 0) printf(" ");
      if (j < n) {
        const char *fmt = format_of(line[j]);
        if (prev_fmt != fmt && option_use_formatting) printf("\x1B[%sm", fmt);
        if (line[j] >= 0x80) printf("%s", CHAR_AREA_HIGH_LUT[line[j] - 0x80]);
        else putchar(isprint(line[j])? line[j] : '.');
        prev_fmt = fmt;
      } else {
        putchar(' ');
      }
    }
    printf("%s\n", option_use_formatting? "\x1B[m" : "");

    memcpy(prev_line, line, n);
    first_line = false;
    if (n < option_columns) break;
  }

  if (ferror(f)) err(1, "(while reading) %s", filename);
}

//-- Entry point ------------
/** Parses a range "start-end" (both ends optional) or "start+size" (neither
 *  optional) into a `struct offset_range` instance. */
struct offset_range parse_range(const char *str) {
  struct offset_range res = { 0, -1 };
  const char *first = str, *delim = str + strcspn(str, "+-"), *second = delim + 1;
  if (*delim == '\0') errx(1, "no delimiter in range %s", str);

  char *end;
  if (first != delim) {
    res.start = strtoimax(first, &end, 0);
    if (!isdigit(*first) || end != delim) errx(1, "invalid start value in range %s", str);
  }
  if (*second != '\0') {
    res.end = strtoimax(second, &end, 0);
    if (!isdigit(*second) || *end != '\0') errx(1, "invalid end/size value in range %s", str);
  }

  if (*delim == '+') {
    if (first == delim) errx(1, "start unspecified in range %s", str);
    if (*second == '\0') errx(1, "size unspecified in range %s", str);
    res.end += res.start;
  }

  if (res.end < res.start && res.end != -1) errx(1, "end was less than start in range %s", str);
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
    hexdump(stdin, "(stdin)");
  } else {
    for (int i = 0; i < argc; i++) {
      FILE *f = fopen(argv[i], "r");
      if (f == NULL) {
        warn("%s", argv[i]);
        continue;
      }

      if (argc > 1) {
        printf("%s====> %s%s%s <====\n", i > 0? "\n" : "",
                                         option_use_formatting? "\x1B[1m" : "",
                                         argv[i],
                                         option_use_formatting? "\x1B[m" : "");
      }

      hexdump(f, argv[i]);
      fclose(f);
    }
  }

  free(colors_var);
  return 0;
}
