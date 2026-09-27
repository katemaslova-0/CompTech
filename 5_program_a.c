/* process_a.c — создаёт ресурсы, вводит число, начинает первым */
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
#define SEM_A    0     // ход A, изначально 1
#define SEM_B    1     // ход B, изначально 0

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
    unsigned short init_vals[2] = {1, 0};   // A=1, B=0 

    // создать разделяемую память под одно int число
    shm_id = shmget(SHM_KEY, sizeof(int), IPC_CREAT | IPC_EXCL | 0600);
    if (shm_id == -1) {
        perror("shmget");
        return 1;
    }

    // создать набор из двух семафоров
    sem_id = semget(SEM_KEY, 2, IPC_CREAT | IPC_EXCL | 0600);
    if (sem_id == -1) {
        perror("semget");
        shmctl(shm_id, IPC_RMID, NULL);
        return 1;
    }

    // инициализировать семафоры: A=1, B=0
    if (semctl(sem_id, 0, SETALL, init_vals) == -1) {
        perror("semctl SETALL");
        return 1;
    }

    // подключаем память
    number = (int *) shmat(shm_id, NULL, 0);
    if (number == (void *) -1) {
        perror("shmat");
        return 1;
    }

    // вводим число
    printf("A: enter the starting number: ");
    fflush(stdout);
    if (scanf("%d", number) != 1) {
        fprintf(stderr, "Bad input\n");
        return 1;
    }
    printf("A: starting. number = %d\n", *number);
    fflush(stdout);

    // ждём свой ход, уменьшаем, передаём ход B 
    while (*number > 0) {
        sem_op(sem_id, SEM_A, -1);      // ждём ход A

        if (*number <= 0) {
            sem_op(sem_id, SEM_B, +1);  // будим B
            break;
        }

        sleep(1);                       // задержка

        *number -= 1;
        printf("A: %d\n", *number);
        fflush(stdout);

        sem_op(sem_id, SEM_B, +1);      // передать ход B 
    }

    // пауза, очистка
    sleep(2);
    shmdt(number);
    semctl(sem_id, 0, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);

    printf("A: done.\n");
    return 0;
}