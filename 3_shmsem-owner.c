#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>

#define SHMEM_SIZE  4096
#define SH_MESSAGE  "Hello World!\n"

#define SEM_KEY     2007
#define SHM_KEY     2007

int main(void)
{
    int   shm_id, sem_id;
    char *shm_buf;
    int   shm_size;
    struct shmid_ds ds;
    struct sembuf   sb[1];
    unsigned short  sem_vals[1];

    shm_id = shmget(SHM_KEY, SHMEM_SIZE, IPC_CREAT | IPC_EXCL | 0600); // создать сегмент разделяемой памяти (если его нет)
    if (shm_id == -1) { 
        perror("shmget");
        return 1;
    }

    sem_id = semget(SEM_KEY, 1, 0600 | IPC_CREAT | IPC_EXCL); // создать набор из 1 семафора
    if (sem_id == -1) {
        perror("semget");
        shmctl(shm_id, IPC_RMID, NULL); // убираем память
        return 1;
    }

    sem_vals[0] = 1; // инициализировать семафор значением 1
    if (semctl(sem_id, 0, SETALL, sem_vals) == -1) {
        perror("semctl SETALL");
        return 1;
    }

    shm_buf = (char *) shmat(shm_id, NULL, 0); // подключить память к процессу 
    if (shm_buf == (char *) -1) {
        perror("shmat");
        return 1;
    }

    shmctl(shm_id, IPC_STAT, &ds); // узнать реальный размер сегмента
    shm_size = ds.shm_segsz;
    if (shm_size < (int)strlen(SH_MESSAGE)) {
        fprintf(stderr, "error: segsize=%d\n", shm_size);
        return 1;
    }

    sb[0].sem_num = 0; 
    sb[0].sem_op  = -1; // захват семафора: 1 -> 0
    sb[0].sem_flg = 0;  // вызов блокирующий
    if (semop(sem_id, sb, 1) == -1) {
        perror("semop lock");
        return 1;
    }

    strcpy(shm_buf, SH_MESSAGE); // пишем в общую память
    printf("Written to shm: %s", shm_buf);

    sb[0].sem_op = +1; // освобождение семафора: 0 -> 1
    if (semop(sem_id, sb, 1) == -1) {
        perror("semop unlock");
        return 1;
    }

    shmdt(shm_buf); // освобождаем ресурсы
    semctl(sem_id, 0, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);

    printf("ID: %d\n", shm_id);
    return 0;
}