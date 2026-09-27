#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>

#define SEM_KEY 2007
#define SHM_KEY 2007
#define SHMEM_SIZE 4096

int main(int argc, char **argv)
{
    int   shm_id, sem_id;
    char *shm_buf;
    struct sembuf sb[1];

    shm_id = shmget(SHM_KEY, SHMEM_SIZE, 0600); // создать сегмент разделяемой памяти (если его нет)
    if (shm_id == -1) {
        perror("shmget");
        return 1;
    }

    sem_id = semget(SEM_KEY, 1, 0600); // ищем набор семафоров
    if (sem_id == -1) {
        perror("semget");
        return 1;
    }

    shm_buf = (char *) shmat(shm_id, NULL, 0); // подключаем память к адресному пространству процесса
    if (shm_buf == (char *) -1) {
        perror("shmat");
        return 1;
    }

    printf("Message: %.100s\n", shm_buf); // прочитать и напечатать содержимое общей памяти

    sb[0].sem_num = 0;
    sb[0].sem_op  = 1;
    sb[0].sem_flg = 0;

    if (semop(sem_id, sb, 1) == -1) { // освободить семафор: 0 -> 1
        perror("semop unlock");
        shmdt(shm_buf);
        return 1;
    }

    shmdt(shm_buf); // отключаем память

    return 0;
}