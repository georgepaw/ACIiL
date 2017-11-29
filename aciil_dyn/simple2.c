#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[])
{
  int N = 50000;
	double a = 1.00000005;
	double b = 0.12;
	double c = 0.0;

	for(int i = 0; i < N; i++)
	{
    c += a + b;
    if(i % 1000 == 0) c *= 1.1;
	}

  for(int i = 0; i < N; i++)
  {
    c *= a;
    if(i % 1000 == 0) printf("c is %lf\n", c);
  }

	printf("c is %lf\n", c);
	return 0;
}
