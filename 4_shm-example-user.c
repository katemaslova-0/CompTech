/* разобраться как работает, написать комментарии,
   в том числе ко всем параметрам. */
#include <sys/shm.h>
#include <stdio.h>

int main (int argc, char ** argv)
{
  int shm_id;     // IPC-дескриптор сегмента разделяемой памяти, который мы получим из аргумента командной строки
  char * shm_buf; // указатель на адрес, по которому сегмент будет отображён в пространство процесса
  
  if (argc < 2) 
  {
  	fprintf (stderr, "Too few arguments\n");
  	return 1;
  }
  
  shm_id = atoi(argv[1]);
  shm_buf = (char *) shmat (shm_id, 0, 0); // подключение сегмента к процессу
  if (shm_buf == (char *) (-1)) 
  {
  	fprintf (stderr, "shmat() error\n");
  	return 1;
  }
  
  printf ("Message: %s\n", shm_buf);
  shmdt (shm_buf); // отсоединяет сегмент от адресного пространства
  
  return 0;
}