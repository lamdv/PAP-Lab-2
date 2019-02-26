#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <omp.h>

#include <x86intrin.h>

#define NBEXPERIMENTS 10000
#define THRESHOLD 4

static long long unsigned int experiments[NBEXPERIMENTS];

static unsigned int N;

typedef int *array_int;

static array_int X;

void init_array(array_int T)
{
  register int i;

  for (i = 0; i < N; i++)
  {
    T[i] = N - i;
  }
}

void print_array(array_int T)
{
  register int i;

  for (i = 0; i < N; i++)
  {
    printf("%d ", T[i]);
  }
  printf("\n");
}

int is_sorted(array_int T)
{
  register int i;

  for (i = 1; i < N; i++)
  {
    /* test designed specifically for our usecase */
    if (T[i - 1] + 1 != T[i])
      return 0;
  }
  return 1;
}

long long unsigned int average(long long unsigned int *exps)
{
  unsigned int i;
  long long unsigned int s = 0;

  for (i = 2; i < (NBEXPERIMENTS - 2); i++)
  {
    s = s + exps[i];
  }

  return s / (NBEXPERIMENTS - 4);
}

void merge(int *T, const int size)
{
  int *X = (int *)malloc(2 * size * sizeof(int));

  int i = 0;
  int j = size;
  int k = 0;

  while ((i < size) && (j < 2 * size))
  {
    if (T[i] < T[j])
    {
      X[k] = T[i];
      i = i + 1;
    }
    else
    {
      X[k] = T[j];
      j = j + 1;
    }
    k = k + 1;
  }

  if (i < size)
  {
    for (; i < size; i++, k++)
    {
      X[k] = T[i];
    }
  }
  else
  {
    for (; j < 2 * size; j++, k++)
    {
      X[k] = T[j];
    }
  }

  memcpy(T, X, 2 * size * sizeof(int));
  free(X);

  return;
}

void merge_sort(int *T, const int size)
{
  if (size < 2)
  {
    return;
  }
  else
  {
    merge_sort(T, size / 2);
    merge_sort(T + size / 2, size / 2);
    merge(T, size / 2);
  }
}

// Vanila version of parallel merge sort
// Naively break in to chunk and merge
// Number of instance: (N-1)!
void parallel_merge_sort(int *T, const int size)
{
  if (size < 2)
  {
    // merge_sort(T, size);
    return;
  }
  else
  {
    {
#pragma omp task firstprivate(T, size)
      parallel_merge_sort(T, size / 2);
#pragma omp task firstprivate(T, size)
      parallel_merge_sort(T + size / 2, size / 2);
#pragma omp taskwait
      merge(T, size / 2);
    }
  }
}

// Improved version of parallel_merge_sort
// Only break down until threshold, then switch to sequential merge sort
// Number of instance: ((N-1)/threshold)!
void parallel_merge_sort_improved(int *T, const int size, int threshold)
{
  if (size < threshold){
    merge_sort(T, size);
    return;
  }
  else
  {
    {
#pragma omp task firstprivate(T, size)
      parallel_merge_sort_improved(T, size / 2, threshold);
#pragma omp task firstprivate(T, size)
      parallel_merge_sort_improved(T + size / 2, size / 2,threshold);
#pragma omp taskwait
      merge(T, size / 2);
    }
  }
}

int main(int argc, char **argv)
{
  omp_set_num_threads(8);
  omp_set_nested(1);
  omp_set_dynamic(1);
  unsigned long long int start, end, residu;
  unsigned long long int av;
  unsigned int exp;
  unsigned int threshold;

  if (argc != 2)
  {
    fprintf(stderr, "mergesort N \n");
    exit(-1);
  }

  N = 1 << (atoi(argv[1]));
  X = (int *)malloc(N * sizeof(int));
  threshold = N / 8;

  printf("--> Sorting an array of size %u\n", N);

  start = _rdtsc();
  end = _rdtsc();
  residu = end - start;

  // print_array (X) ;

  printf("sequential sorting ...\n");

  for (exp = 0; exp < NBEXPERIMENTS; exp++)
  {
    init_array(X);

    start = _rdtsc();

    merge_sort(X, N);

    end = _rdtsc();
    experiments[exp] = end - start;

    if (!is_sorted(X))
    {
      fprintf(stderr, "ERROR: the array is not properly sorted\n");
      exit(-1);
    }
  }

  av = average(experiments);
  printf("\n merge sort serial\t\t %Ld cycles\n\n", av - residu);

  printf("parallel sorting ...\n");

  for (exp = 0; exp < NBEXPERIMENTS; exp++)
  {
    init_array(X);

    start = _rdtsc();
    #pragma omp parallel
    {
      #pragma omp single
      parallel_merge_sort(X, N);
    }

    end = _rdtsc();
    experiments[exp] = end - start;

    if (!is_sorted(X))
    {
      fprintf(stderr, "ERROR: the array is not properly sorted\n");
      print_array(X);
      exit(-1);
    }
  }

  av = average(experiments);
  printf("\n merge sort parallel with tasks\t %Ld cycles\n\n", av - residu);

  printf("parallel sorting with improved version...\n");

  for (exp = 0; exp < NBEXPERIMENTS; exp++)
  {
    init_array(X);

    start = _rdtsc();
    #pragma omp parallel
    {
      #pragma omp single
      parallel_merge_sort_improved(X, N, threshold);
    }

    end = _rdtsc();
    experiments[exp] = end - start;

    if (!is_sorted(X))
    {
      fprintf(stderr, "ERROR: the array is not properly sorted\n");
      print_array(X);
      exit(-1);
    }
  }

  av = average(experiments);
  printf("\n improved merge sort parallel with tasks\t %Ld cycles\n\n", av - residu);
}
