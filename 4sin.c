#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

#define N_TERMS 20 // cколько членов ряда Тейлора считать (влияет на точность)
static double X = 1.0; // аргумент, для которого считаем синус

/* ---------- Разделяемые данные ---------- */

// Частичные суммы от каждой нити
static double * partial_sums = NULL;
static int      n_threads    = 1;

/* ---------- Вспомогательные функции ---------- */

/* Разность timespec в секундах */
static double ts_diff (struct timespec * a, struct timespec * b)
{
  return (b->tv_sec - a->tv_sec) +
         (b->tv_nsec - a->tv_nsec) / 1e9;
}

/* Ряд Тейлора для sin(x): сумма от k=0 до N-1 членов.
   Член k: (-1)^k * x^(2k+1) / (2k+1)!
   Нить с номером id обрабатывает члены k = id, id + n_threads, id + 2*n_threads, ... */
void * worker (void * arg)
{
  int id = *(int *) arg;
  double term = 0.0;
  double sum  = 0.0;

  /* Вычислим x^(2*id+1) / (2*id+1)! для первого своего члена,
     а дальше будем обновлять рекуррентно:
        term_{k+1} = term_k * (-x*x) / ((2k+2)*(2k+3)) */
  double x_pow = pow (X, 2 * id + 1);
  double fact  = 1.0;
  for (int i = 2; i <= 2 * id + 1; i++) fact *= i;
  term = x_pow / fact;
  if (id % 2 == 1) term = -term;

  for (int k = id; k < N_TERMS; k += n_threads)
  {
    sum += term;

    // Переход к следующему члену k+1 (только если он ещё наш)
    int k_next = k + n_threads;
    if (k_next < N_TERMS)
    {
      /* Домножаем на x^2 и делим на (2k+2)(2k+3) с учётом знака.
         но так как шаг = n_threads, между "нашими" членами стоит
         (2*n_threads) множителей — учитываем это */
      for (int s = 0; s < n_threads; s++)
      {
        int kk = k + s;
        term *= -X * X / ((2.0 * kk + 2.0) * (2.0 * kk + 3.0));
      }
    }
  }

  partial_sums[id] = sum;
  return NULL;
}

/* ---------- main ---------- */

int main (int argc, char ** argv)
{
  if (argc < 2)
  {
    fprintf (stderr, "Использование: %s <число потоков> [x] [N_terms]\n",
             argv[0]);
    return 1;
  }
  n_threads = atoi (argv[1]);
  if (n_threads < 1) n_threads = 1;
  if (argc >= 3) X = atof (argv[2]);

  partial_sums = calloc (n_threads, sizeof (double));
  if (!partial_sums) { perror ("calloc"); return 1; }

  pthread_t * tids = malloc (n_threads * sizeof (pthread_t));
  int       * ids  = malloc (n_threads * sizeof (int));
  if (!tids || !ids) { perror ("malloc"); return 1; }

  for (int i = 0; i < n_threads; i++) ids[i] = i;

  // Замер времени
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int i = 0; i < n_threads; i++)
  {
    if (pthread_create (&tids[i], NULL, &worker, &ids[i]) != 0)
    {
      fprintf (stderr, "Ошибка pthread_create\n");
      return 1;
    }
  }
  for (int i = 0; i < n_threads; i++)
    pthread_join (tids[i], NULL);

  clock_gettime (CLOCK_MONOTONIC, &t1);
  double elapsed = ts_diff (&t0, &t1);

  // Суммируем частичные суммы
  double my_sin = 0.0;
  for (int i = 0; i < n_threads; i++) my_sin += partial_sums[i];

  double std_sin = sin (X);
  double err_abs = fabs (my_sin - std_sin);
  double err_rel = (std_sin != 0.0) ? err_abs / fabs (std_sin) : err_abs;

  printf ("x = %.6f, N_terms = %d, потоков = %d\n", X, N_TERMS, n_threads);
  printf ("  ряд Тейлора : %.15f\n", my_sin);
  printf ("  стандарт sin: %.15f\n", std_sin);
  printf ("  абс. ошибка : %.3e\n", err_abs);
  printf ("  отн. ошибка : %.3e\n", err_rel);
  printf ("  время       : %.6f с\n", elapsed);

  free (partial_sums);
  free (tids);
  free (ids);
  return 0;
}