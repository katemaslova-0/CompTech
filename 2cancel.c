#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#define NUM_ITERATIONS 5

void * any_func (void * arg)
{
  while (1) 
  {
    fprintf (stderr, ".");
    sleep (10);
  }
  
  return NULL;
}

// Вспомогательная функция: разница между двумя timespec в секундах (double)
double timespec_diff (struct timespec * start, struct timespec * end)
{
  return (end->tv_sec - start->tv_sec) +
         (end->tv_nsec - start->tv_nsec) / 1e9;
}

int main (void)
{
  double total_time = 0.0;
  int i;

  for (i = 0; i < NUM_ITERATIONS; i++)
  {
    pthread_t thread;
    void * result;
    struct timespec t_signal, t_joined;

    if (pthread_create (&thread, NULL, &any_func, NULL) != 0) 
    {
      fprintf (stderr, "Error\n");
      return 1;
    }

    sleep (5);

    // Фиксируем время непосредственно перед отправкой сигнала на отмену 
    clock_gettime (CLOCK_MONOTONIC, &t_signal);
    pthread_cancel (thread);

    if (!pthread_equal (pthread_self (), thread))
    {
      pthread_join (thread, &result);
    }

    // Фиксируем время сразу после завершения нити
    clock_gettime (CLOCK_MONOTONIC, &t_joined);

    double elapsed = timespec_diff (&t_signal, &t_joined);
    total_time += elapsed;

    if (result == PTHREAD_CANCELED)
    {
      fprintf (stderr, "\nИтерация %d: отменена, время завершения = %.6f с\n",
               i + 1, elapsed);
    }
  }

  fprintf (stderr, "\nСреднее время завершения после сигнала на отмену: %.6f с\n",
           total_time / NUM_ITERATIONS);

  return 0;
}