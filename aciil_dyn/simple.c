#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[])
{
  int N = 1001;
	double a = 2.45;
	double b = 3.12;
	double c = 0.0;

	for(int i = 0; i < N; i++)
	{
    c += a + b;
    if(i % 10) printf("i is %d c is %lf\n", i, c);
	}

	printf("c is %lf\n", c);
	return 0;
}
