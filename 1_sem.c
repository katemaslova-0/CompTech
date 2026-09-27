#include <sys/types.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <stdio.h>

int main(int argc, char *argv[], char *envp[])
{
  int semid;
  char pathname[]="1_sem.c";                         // файл, к которому привязывается ключ
  key_t key;
  struct sembuf mybuf;
  
  key = ftok(pathname, 0);                           // формирует key_t-ключ из пути к файлу и номера проекта
  
  if((semid = semget(key, 1, 0666 | IPC_CREAT)) < 0) // 1 - количество семафоров, 0666 - права доступа
  {                                                  // IPC_CREAT - создать набор, если его нет
    printf("Can\'t create semaphore set\n");
    exit(-1);
  }

  if (semctl(semid, 0, SETVAL, 1) < 0) {            // инициализация семафора в 1
        printf("Can't initialize semaphore\n");
        exit(-1);
    }
  
  mybuf.sem_num = 0;                                // индекс семафора в наборе
  mybuf.sem_op  = -1;                               // атомарно вычесть 1
  mybuf.sem_flg = 0;                                // вызов блокирующий(ждать, если нельзя выполнить операцию)
  
  if(semop(semid, &mybuf, 1) < 0)                   // операция над семафором
  {
    printf("Can\'t wait for condition\n");
    exit(-1);
  }  
    
  printf("The condition is present\n");
  return 0;
}