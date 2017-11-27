#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[])
{
  int N = 50001;
	double a = 0.45;
	double b = 0.12;
	double c = 0.0;

	for(int i = 0; i < N; i++)
	{
    c += a + b;
    if(i % 10 == 0) c /= 2.0;
	}

  for(int i = 0; i < N; i++)
  {
    b += c/a;
    if(i % 10 == 0) c -= c/10.0;
  }

	printf("c is %lf\n", c);
	return 0;
}
