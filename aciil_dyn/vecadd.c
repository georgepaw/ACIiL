#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
  int N = 1003;
  int itterations = 30000;
  double a[N];
  double b[N];
  double c[N];

  for (int i = 0; i < N; i++) {
    c[i] = 0.0;
    a[i] = 0.0015;
    b[i] = 0.002;
  }

  for (int i = 0; i < itterations; i++) {
    printf("i is %d\n", i);
    for (int j = 0; j < N; j++)
      c[j] += a[j] + b[j];
  }

  char passed = 1;
  for (int j = 0; j < N; j++) {
    double diff = c[j] - (double)itterations * (a[j] + b[j]);
    diff *= diff < 0.0 ? -1.0 : 1.0;
    if (diff > 0.00001)
      passed = 0;
  }

  printf("Passed %u\n", passed);

  printf("c is %lf\n", c[2]);
  return 0;
}