/* подключается к готовым ресурсам, работает вторым */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SHM_KEY  2007
#define SEM_KEY  2007
#define SEM_A    0
#define SEM_B    1

// обёртка над semop с обработкой EINTR
static void sem_op(int sem_id, int num, int op)
{
    struct sembuf sb;
    sb.sem_num = num;
    sb.sem_op  = op;
    sb.sem_flg = 0;
    while (semop(sem_id, &sb, 1) == -1) {
        if (errno != EINTR) {
            perror("semop");
            exit(1);
        }
    }
}

int main(void)
{
    int shm_id, sem_id;
    int *number;

    // открыть существующую память, сделанную А
    shm_id = shmget(SHM_KEY, sizeof(int), 0600);
    if (shm_id == -1) {
        perror("shmget (is process A running?)");
        return 1;
    }

    // открыть существующий набор семафоров
    sem_id = semget(SEM_KEY, 2, 0600);
    if (sem_id == -1) {
        perror("semget");
        return 1;
    }

    // подключаемся к памяти
    number = (int *) shmat(shm_id, NULL, 0);
    if (number == (void *) -1) {
        perror("shmat");
        return 1;
    }

    printf("B: waiting for the first move from A...\n");
    fflush(stdout);

    // ждём свой ход (A разбудит нас после ввода числа), уменьшаем, передаем ход А
    for (;;) {
        sem_op(sem_id, SEM_B, -1);      // ждём ход B

        if (*number <= 0) {
            sem_op(sem_id, SEM_A, +1);  // будим A, чтобы он тоже вышел 
            break;
        }

        sleep(1);                       // задержка

        *number -= 1;
        printf("B: %d\n", *number);
        fflush(stdout);

        sem_op(sem_id, SEM_A, +1);      // передаем ход А
    }

    shmdt(number);
    printf("B: done.\n");
    return 0;
}