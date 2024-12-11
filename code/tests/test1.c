#include <stdio.h>
#include <stdlib.h>

int main() {
  int id = 5;
  int n = 10;
  scanf("%d, %d", &id, &n);
  unsigned long long s = 1;
  for (int i = 0; i < n; i++) {
    s += rand();
  }
  printf("id = %d; iter = %d, mult = %llu\n", id, n, s);
}