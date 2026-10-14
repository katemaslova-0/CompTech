#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

// Аргумент для нитей - угол и куда положить результат 
struct thread_arg
{
  double   x;       // угол в радианах
  double   result;  
};

void * thread_sin (void * arg)
{
  struct thread_arg * a = (struct thread_arg *) arg;
  a->result = sin (a->x) * sin (a->x);   // sin^2(x)
  fprintf (stderr, "thread_sin: sin^2(%.3f) = %.15f\n", a->x, a->result);
  return NULL;
}

void * thread_cos (void * arg)
{
  struct thread_arg * a = (struct thread_arg *) arg;
  a->result = cos (a->x) * cos (a->x);   // cos^2(x)
  fprintf (stderr, "thread_cos: cos^2(%.3f) = %.15f\n", a->x, a->result);
  return NULL;
}

int main (void)
{
  pthread_t thread_sin_id, thread_cos_id;
  struct thread_arg sin_arg, cos_arg;

  // Возьмём произвольный угол
  double x = 0.7;

  sin_arg.x = x;
  cos_arg.x = x;

  if (pthread_create (&thread_sin_id, NULL, &thread_sin, &sin_arg) != 0)
  {
    fprintf (stderr, "Error (thread_sin)\n");
    return 1;
  }

  if (pthread_create (&thread_cos_id, NULL, &thread_cos, &cos_arg) != 0)
  {
    fprintf (stderr, "Error (thread_cos)\n");
    return 1;
  }

  // Ждём завершения обеих нитей, чтобы результаты были готовы
  pthread_join (thread_sin_id, NULL);
  pthread_join (thread_cos_id, NULL);

  // Суммируем в главной программе
  double sum = sin_arg.result + cos_arg.result;

  fprintf (stderr, "\nmain: sin^2(x) = %.15f\n", sin_arg.result);
  fprintf (stderr, "main: cos^2(x) = %.15f\n", cos_arg.result);
  fprintf (stderr, "main: sin^2(x) + cos^2(x) = %.15f\n", sum);

  // Проверка с учётом погрешности вычислений с плавающей точкой
  if (fabs (sum - 1.0) < 1e-12)
    fprintf (stderr, "main: тождество выполняется (в пределах погрешности)\n");
  else
    fprintf (stderr, "main: расхождение слишком велико\n");

  return 0;
}