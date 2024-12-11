#include <stdio.h>

int main() {
  char str1[1000];
  FILE *fp = fopen("../tests/words.txt", "r");
  char c;
  int len = 0;
  int blah = 0;
  while (1) {
    c = getc(fp);
    if (c == EOF)
      break;
    str1[len++] = c;
    if (len >= 1000)
      break;
  }
  printf("%s\n", str1);
}