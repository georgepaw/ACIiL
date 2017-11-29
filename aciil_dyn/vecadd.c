#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
  int N = 1001;
  int itterations = 30000;
  double a[N];
  double b[N];
  double c[N];

  for (int i = 0; i < N; i++) {
    a[i] = 1.0;
    b[i] = 2.0;
  }

  for (int i = 0; i < itterations; i++) {
    for (int j = 0; j < N; j++)
      c[j] = a[j] + b[j];
  }

  // char passed = 1;
  // for(int j = 0; j < N; j++)
  //   if(c[j] != a[j] + b[j]) passed = 0;

  // printf("Passed %u\n", passed);

  printf("c is %lf\n", c[2]);
  return 0;
}