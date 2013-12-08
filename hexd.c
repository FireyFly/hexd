#include <sys/ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FMT_SIZE 16


/**** Options ********************************************/
struct range { unsigned int min; unsigned int max; };
struct {
  int colors : 1;     /* use colours? */
  int columns;        /* octets per line */
  int group_size;     /* octets per group */
  struct range range; /* range to print for each file */

} options = {
  .colors     =  1,
  .columns    = -1,
  .group_size =  8,
  .range      = { 0, -1 }
};

struct {
  char byte_zero[FMT_SIZE];      /*       0 */
  char byte_low[FMT_SIZE];       /*   1- 31 */
  char byte_printable[FMT_SIZE]; /*  32-126 */
  char byte_high[FMT_SIZE];      /* 127-255 */

} colors = {
  .byte_zero       = "1;30",
  .byte_low        = "32",
  .byte_printable  = "",
  .byte_high       = "31"
};


/**** Hexdump ********************************************/
int is_printable(char ch) {
  return ' ' <= ch && ch <= '~';
}
int is_alphanumeric(char ch) {
  return ('0' <= ch && ch <= '9') ||
         ('A' <= ch && ch <= 'Z') ||
         ('a' <= ch && ch <= 'z');
}

void put_fmt(char *sgr) {
  if (!options.colors) return;

  if (sgr == NULL) printf("\033[m");
  else             printf("\033[%sm", sgr);
}

void hexdump_line_raw(uint8_t *buf, int len) {
  int gsize = options.group_size;
  uint8_t ch;

  /* Print hex part */
  for (int i=0; i<len; i++) {
    ch = buf[i];
    if (gsize != 0 && i % gsize == 0) putchar(' ');

         if (ch == 0)   put_fmt(colors.byte_zero);
    else if (ch <  ' ') put_fmt(colors.byte_low);
    else if (ch >  '~') put_fmt(colors.byte_high);
    else                put_fmt(colors.byte_printable);

    printf(" %02x", ch);
    put_fmt(NULL);
  }

  /* Pad if not enough octets to fill a full line */
  for (int i=len; i < options.columns; i++) {
    if (gsize != 0 && i % gsize == 0) putchar(' ');
    printf("   ");
  }

  putchar(' ');

  /* Print char part */
  for (int i=0; i<len; i++) {
    ch = buf[i];
    if (gsize != 0 && i % gsize == 0) putchar(' ');

         if (ch == 0)   put_fmt(colors.byte_zero);
    else if (ch <  ' ') put_fmt(colors.byte_low);
    else if (ch >  '~') put_fmt(colors.byte_high);
    else                put_fmt(colors.byte_printable);

    putchar(is_printable(ch)? ch : '.');
    put_fmt(NULL);
  }

  putchar('\n');
}

void hexdump_line(uint8_t *buf, int len, int i) {
  uint32_t offset = i - ((i - 1) % options.columns) - 1;  /* TODO */
  printf("%5x%03x", offset >> 12, offset & 0xFFF);
  hexdump_line_raw(buf, len);
}

void hexdump(FILE *f) {
  int columns = options.columns;

  int ch, i = 0;
  while (i++ < options.range.min && (ch = fgetc(f)) != EOF);
  i--;

  uint8_t buf[columns];

  int j = 0;
  int all_same = 0, has_printed_asterisk = 0;
  while (i++ < options.range.max && (ch = fgetc(f)) != EOF) {
    if (buf[j % columns] != ch) all_same = 0;
    buf[j++ % columns] = ch;

    if (j % columns == 0) {
      if (all_same) {
        if (!has_printed_asterisk) {
          printf("%8s\n", "*");
          has_printed_asterisk = 1;
        }
      } else {
        has_printed_asterisk = 0;
        hexdump_line(buf, columns, i);
      }

      all_same = 1;
    }
  }

  if (j % columns != 0) hexdump_line(buf, (j % columns), i);
}


/**** Helpers ********************************************/
void print_version_and_exit() {
  printf("FireFly's hexd (0.0)\n");
  exit(0);
}
void print_help_and_exit(char *name) {
  printf(
    "Usage: %s [OPTION]... [FILE]...\n"
    "A pretty-printed (human-readable) hexdump alternative.\n"
    "\n"
    "Available flags.\n"
    "  -w COUNT          print COUNT octets per line\n"
    "  -g SIZE           group output into SIZE-octet groups (default: 8; 0"
                       " disables grouping)\n"
    "  -r [FROM],[TO]    print only bytes between FROM (incl.) and TO (excl.)\n"
    "  -p                plain output, don't format\n"
    "  -h                print this help\n"
    "  -v                print version information\n",
         name);
  exit(0);
}

int parse_int(char *str) {
  int radix = 10,
      res   = 0;

  char *p = str;

  if (p[0] == '0' && p[1] == 'x') {
    radix = 16;
    p += 2;
  }

  while (*p++) {
    res = res * radix + (*(p - 1) - '0');
  }

  return res;
}

struct range parse_range(char *str) {
  struct range range = { 0, -1 };

  char *min = str, *max;
  for (max = min; *max != ',' && *max != '\0'; max++);

  if (*max != ',') {
    fprintf(stderr, "Bad range: '%s' (didn't find any ',')\n", str);
    exit(2);
  }

  *max++ = '\0';

  if (strcmp(min, "^") != 0 && *min != '\0') range.min = parse_int(min);
  if (strcmp(max, "$") != 0 && *max != '\0') range.max = parse_int(max);
  return range;
}


/**** Entry point ****************************************/
/* Processes flags and sets the relevant options in `options`.  Returns start
 * index of rest-of-arguments (in `argv`). */
int process_flags(int argc, char *argv[]) {
  char *arg, *value;
  int i, j;

  for (i=1; i < argc; i++) {
    arg = argv[i];

    if (arg[0] == '-') {
      if (arg[1] == '\0') return i;  /* - signifies the stdin file */

      value = arg[2] != '\0'? &arg[2] : argv[++i];

      switch (arg[1]) {
        case '\0':
          return i;  /* - signifies stdin file */

        case '-':
          if (arg[2] == '\0') {
            return i + 1;  /* -- signifies end-of-flags */
          } else {
            fprintf(stderr, "%s: long flags not supported.\n", argv[0]);
            exit(2);
          }

        case 'h': print_help_and_exit(argv[0]);
        case 'v': print_version_and_exit();
        case 'w': options.columns    = parse_int(value);   break;
        case 'g': options.group_size = parse_int(value);   break;
        case 'r': options.range      = parse_range(value); break;
        case 'p': options.colors     = 0;                  break;

        default:
          fprintf(stderr, "%s: flag '-%c' not supported.\n", argv[0], arg[1]);
          exit(2);
      }

    /* Not a flag; reached end-of-flags */
    } else {
      return i;
    }
  }

  return i;
}

/* Processes colour environment variable and updates `color` with new values. */
void process_colors(char *pairs) {
  char *p = pairs;

  /* name=value
     ↑    ↑
     n    s */
  char *name_start, *value_start;
  size_t value_length;

  name_start = p;

  do {
    switch (*p) {
      case '=':
        value_start = p+1;
        break;

      case ',': case ' ': case '\n': case '\t': case '\0':
        value_length = (p - value_start) / sizeof(char);

        /* Ignore empty fields */
        if (!is_alphanumeric(name_start[0])) break;

        if (value_length >= FMT_SIZE) {
          fprintf(stderr, "hexd: too long SGR format value.\n");
          return;
        }

        char *dest;

        switch (name_start[0] << 8 | name_start[1]) {
          case 'z' << 8 | 'e': dest = colors.byte_zero;      break;
          case 'l' << 8 | 'o': dest = colors.byte_low;       break;
          case 'p' << 8 | 'r': dest = colors.byte_printable; break;
          case 'h' << 8 | 'i': dest = colors.byte_high;      break;

          default:
            value_start[-1] = '\0';
            fprintf(stderr, "hexd: unknown field: '%s'.\n", name_start);
            return;
        }

        strncpy(dest, value_start, value_length);
        dest[value_length] = '\0';

        name_start = p + 1;
    }
  } while (*p++);
}

int main(int argc, char *argv[]) {
  int files_idx = process_flags(argc, argv);

  /* Handle environment variable 'HEXD_COLORS' */
  char *colors_var = getenv("HEXD_COLORS");
  if (colors_var != NULL) process_colors(colors_var);

  /* Set TTY defaults if applicable */
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  int has_tty = isatty(fileno(stdout));
  int gs = options.group_size, computed_columns;

  if (gs == 0) {
    computed_columns = (w.ws_col - 9) / 4;
  } else {
    computed_columns = ((w.ws_col - 9) / (4*gs + 2)) * gs;
  }

  if (options.columns == -1) {
    options.columns = has_tty? computed_columns : 16;
  }

  /* Do the hexdumping */
  if (files_idx < argc) {
    for (int i = files_idx; i < argc; i++) {
      FILE *f = strcmp(argv[i], "-") == 0?  stdin
              : /*else*/                    fopen(argv[i], "r");

      if (argc - files_idx > 1) {
        printf("%s====> \033[1m%s\033[m <====\n", i > files_idx? "\n" : "", argv[i]);
      }

      hexdump(f);
      fclose(f);
    }

  } else {
    /* No files given; read from stdin. */
    hexdump(stdin);
  }

  return 0;
}
