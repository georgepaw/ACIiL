//
// Implementation of the iterative Jacobi method.
//
// Given a known, diagonally dominant matrix A and a known vector b, we aim to
// to find the vector x that satisfies the following equation:
//
//     Ax = b
//
// We first split the matrix A into the diagonal D and the remainder R:
//
//     (D + R)x = b
//
// We then rearrange to form an iterative solution:
//
//     x' = (b - Rx) / D
//
// More information:
// -> https://en.wikipedia.org/wiki/Jacobi_method
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define SEPARATOR "------------------------------------\n"

// Return the current time in seconds since the Epoch
double get_timestamp();

// Parse command line arguments to set solver parameters
void parse_arguments(int argc, char *argv[]);

// Run the Jacobi solver
// Returns the number of iterations performed
int run(double *A, double *b, double *x, double *xtmp, const int N,
        const int MAX_ITERATIONS, const double CONVERGENCE_THRESHOLD) {
  int itr;
  int row, col;
  double dot;
  double diff;
  double sqdiff;
  double *ptrtmp;

  // Loop until converged or maximum iterations reached
  itr = 0;
  do {
    if (itr && itr % 10 == 0)
      printf("Itr is %d\n", itr);
    // Perfom Jacobi iteration
    for (row = 0; row < N; row++) {
      dot = 0.0;
      for (col = 0; col < N; col++) {
        if (row != col)
          dot += A[row + col * N] * x[col];
      }
      xtmp[row] = (b[row] - dot) / A[row + row * N];
    }

    // Swap pointers
    ptrtmp = x;
    x = xtmp;
    xtmp = ptrtmp;

    // Check for convergence
    sqdiff = 0.0;
    for (row = 0; row < N; row++) {
      diff = xtmp[row] - x[row];
      sqdiff += diff * diff;
    }

    itr++;
  } while ((itr < MAX_ITERATIONS) && (sqrt(sqdiff) > CONVERGENCE_THRESHOLD));

  return itr;
}

int main(int argc, char *argv[]) {
  // Set default values
  const int N = atoi(argv[1]);
  const int MAX_ITERATIONS = 20000;
  const int SEED = 0;
  const double CONVERGENCE_THRESHOLD = 0.0001;

  double *A = malloc(sizeof(double) * N * N);
  double *b = malloc(sizeof(double) * N);
  double *x = malloc(sizeof(double) * N);
  double *xtmp = malloc(sizeof(double) * N);

  printf(SEPARATOR);
  printf("Matrix size:            %dx%d\n", N, N);
  printf("Maximum iterations:     %d\n", MAX_ITERATIONS);
  printf("Convergence threshold:  %lf\n", CONVERGENCE_THRESHOLD);
  printf(SEPARATOR);

  double total_start = get_timestamp();

  // Initialize data
  srand(SEED);
  for (int row = 0; row < N; row++) {
    double rowsum = 0.0;
    for (int col = 0; col < N; col++) {
      double value = rand() / (double)RAND_MAX;
      A[row + col * N] = value;
      rowsum += value;
    }
    A[row + row * N] += rowsum;
    b[row] = rand() / (double)RAND_MAX;
    x[row] = 0.0;
  }

  // Run Jacobi solver
  double solve_start = get_timestamp();
  printf("Solver start\n");
  int itr = run(A, b, x, xtmp, N, MAX_ITERATIONS, CONVERGENCE_THRESHOLD);
  printf("Solver done\n");
  for (int col = 0; col < N; col++) {
    printf("%lf ", x[col]);
  }
  printf("\n");
  double solve_end = get_timestamp();

  // Check error of final solution
  double err = 0.0;
  for (int row = 0; row < N; row++) {
    double tmp = 0.0;
    for (int col = 0; col < N; col++) {
      tmp += A[row + col * N] * x[col];
      // printf("row %d col %d A %lf x %lf xtmp %lf\n", row, col, A[row + col *
      // N], x[col], xtmp[col]);
    }
    // printf("row %d b %lf\n", row, b[row]);
    tmp = b[row] - tmp;
    err += tmp * tmp;
  }
  err = sqrt(err);

  double total_end = get_timestamp();

  printf("Solution error = %lf\n", err);
  printf("Iterations     = %d\n", itr);
  printf("Total runtime  = %lf seconds\n", (total_end - total_start));
  printf("Solver runtime = %lf seconds\n", (solve_end - solve_start));
  if (itr == MAX_ITERATIONS)
    printf("WARNING: solution did not converge\n");
  printf(SEPARATOR);

  return 0;
}

double get_timestamp() {
  // struct timeval tv;
  // gettimeofday(&tv, NULL);
  return 0; // tv.tv_sec + tv.tv_usec * 1e-6;
}
