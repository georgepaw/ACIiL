#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
  int N = 50000000;
  double a = 1.0000000000000000000000000000005;
  double b = 0.0000000000000000000000000000012;
  double c = 0.0;

  for (int i = 0; i < N; i++) {
    c += a + b;
    if (i % 1000 == 0)
      c *= 1.0000000000000000000000000000001;
  }

  for (int i = 0; i < N; i++) {
    c += a;
    if (i % 1000 == 0)
      printf("i is %d and c is %lf\n", i, c);
  }

  printf("c is %lf\n", c);
  return 0;
}
