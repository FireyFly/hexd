#include <stdio.h>

/* Filter that strips control sequences (initiated by CSI, e.g. \x1B[ or \x9B)
 * while copying stdin to stdout.
 */
int main(void) {
  enum state { STATE_PLAIN, STATE_ESC, STATE_CSI };

  int state = STATE_PLAIN;
  int ch;

  while (ch = getchar(), ch != EOF) {
    switch (state) {
      case STATE_PLAIN:
        if (ch == '\x1B') {
          state = STATE_ESC;
        } else if (ch == '\x9B') {
          state = STATE_CSI;
        } else {
          putchar(ch);
        }
        break;

      case STATE_ESC:
        if (ch == '[') {
          state = STATE_CSI;
        } else {
          putchar('\x1B');
          putchar(ch);
          state = STATE_PLAIN;
        }
        break;

      case STATE_CSI:
        /* Parameter bytes */
        while (0x30 <= ch && ch <= 0x3F && ch != EOF) ch = getchar();

        /* Intermediate bytes */
        while (0x20 <= ch && ch <= 0x2F && ch != EOF) ch = getchar();

        /* Final byte */
        if (!(0x40 <= ch && ch <= 0x7E)) {
          fprintf(stderr, "Warning: invalid control sequence terminating in %02x\n", ch);
        }
        state = STATE_PLAIN;
        break;
    }
  }

  return 0;
}
