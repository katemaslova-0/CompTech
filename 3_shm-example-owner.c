/* Проверить совместную работу с 4.
   Написать комментарии, В ТОМ ЧИСЛЕ К ПАРАМЕТРАМ!*/
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>

// размер запрашиваемого сегмента
#define SHMEM_SIZE	4096
// строка, которую кладем в разделяемую память
#define SH_MESSAGE	"Poglad Kota!\n"

int main (void)
{
  int shm_id;          // IPC дескриптор сегмента
  char * shm_buf;      // адрес, по которому сегмент отображается в адресное пространство процесса.
  int shm_size;        // размер сегмента
  struct shmid_ds ds;  // структура с информацией о сегменте (struct shmid_ds)
  
  shm_id = shmget (IPC_PRIVATE,
                   SHMEM_SIZE,
  		           IPC_CREAT | IPC_EXCL | 0600); // создаёт новый сегмент размера SHMEM_SIZE
                                                 // IPC_CREAT - создать, если не существует
                                                 // IPC_EXCL - выдаст ошибку, если уже существует
                                                 // 0600 - права доступа
  if (shm_id == -1)
  {
    fprintf (stderr, "shmget() error\n");
    return 1;
  }
  
  shm_buf = (char *) shmat (shm_id,  // подключение сегмента к процессу, возращает указатель, по которому
                            NULL,    // можно к нему обратиться; 
                            0);
  if (shm_buf == -1)
  {
    fprintf (stderr, "shmat() error\n");
  	return 1;
  }
  
  shmctl (shm_id,                    // читает структуру shmid_ds в ds
          IPC_STAT,
          &ds);
  
  shm_size = ds.shm_segsz;           // фактический размер сегмента
  if (shm_size < strlen (SH_MESSAGE)) 
  {
  	fprintf (stderr, "error: segsize=%d\n", shm_size);
  	return 1;
  }
  
  strcpy (shm_buf,
          SH_MESSAGE);              // копируем строку SH_MESSAGE в разделяемую память shm_buf
  
  printf ("ID: %d\n", shm_id);
  printf ("Press <Enter> to exit...");	
  fgetc (stdin);
  
  shmdt (shm_buf);                 // отсоединяем сегмент от адресного пространства
  shmctl(shm_id,                   // пометить сегмент на удаление
         IPC_RMID,
         NULL);
  
  return 0;
}