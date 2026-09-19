#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <sys/types.h>

// Проблемы программы, влияющие на надежность: 
// 1) несколько процессов пишут и читают без всякой синхронизации (исправлено с flock)
// 2) ошибка в строке scanf("%d", shm_key) - нужен адрес
// 3) в строке shmctl(shmid_test, IPC_STAT, *buf) разыменовывается мусорный buf; также нужен адрес

#define SHM_KEY_BASE 0xDEADBABE
#define N_SLOTS   64
#define NAME_MAX  32
#define TEXT_MAX  256

// Одно сообщение в общей памяти.
struct msg {
    int  used;                      // 0 — слот пуст, 1 — есть сообщение
    int  seq;                       // порядковый номер, чтобы читатель не читал дважды
    char from[NAME_MAX];            // имя отправителя
    char to[NAME_MAX];              // имя получателя
    char text[TEXT_MAX];            // текст сообщения
};

// Чат
struct chat {
    int       head;                 // индекс следующего слота для записи
    int       seq;                  // счётчик всех сообщений (для seq в msg)
    int       nusers;               // сколько сейчас подключено
    struct msg slots[N_SLOTS];
};

static int           shmid   = -1;      // дескриптор сегмента
static struct chat  *shm     = NULL;    // указатель на сегмент
static int           lock_fd = -1;      // файл-замок для синхронизации 
static char          me[NAME_MAX];      // моё имя 
static int           my_last_seq = 0;   // до какого seq я уже прочитал 
static volatile sig_atomic_t stop = 0;  // флаг завершения 

// Синхронизация через flock

static void lock(void) {
    if (flock(lock_fd, LOCK_EX) == -1) {
        perror("flock LOCK_EX");
        exit(1);
    }
}

static void unlock(void) {
    if (flock(lock_fd, LOCK_UN) == -1) {
        perror("flock LOCK_UN");
        exit(1);
    }
}

// Запись сообщения в буфер

static void put_msg_locked(const char *to, const char *text) {
    struct msg *m = &shm->slots[shm->head];
    m->used = 1;
    m->seq  = ++shm->seq;
    strncpy(m->from, me,     NAME_MAX - 1); m->from[NAME_MAX - 1] = '\0';
    strncpy(m->to,   to,     NAME_MAX - 1); m->to  [NAME_MAX - 1] = '\0';
    strncpy(m->text, text,   TEXT_MAX - 1); m->text[TEXT_MAX - 1] = '\0';
    shm->head = (shm->head + 1) % N_SLOTS;
}

// Завершение

static void cleanup(void) {
    if (shm && lock_fd != -1) {
        lock();
        put_msg_locked("*", "*** left the chat");
        shm->nusers--;
        unlock();
    }

    if (shm) {
        shmdt(shm);
        shm = NULL;
    }

    if (shmid != -1) {
        struct shmid_ds ds;
        if (shmctl(shmid, IPC_STAT, &ds) == 0 && ds.shm_nattch == 0) {
            shmctl(shmid, IPC_RMID, NULL);
        }
    }

    if (lock_fd != -1) close(lock_fd);
}

static void on_signal(int sig) {
    (void)sig;
    stop = 1;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <chat_no> <username>\n", argv[0]);
        return 1;
    }

    int chat_no = atoi(argv[1]);
    strncpy(me, argv[2], NAME_MAX - 1);
    me[NAME_MAX - 1] = '\0';

    key_t key = SHM_KEY_BASE + chat_no;

    // Создаём или находим сегмент
    size_t sz = sizeof(struct chat);
    shmid = shmget(key, sz, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        return 1;
    }

    shm = (struct chat *) shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        return 1;
    }

    // Файл-замок: один общий для всех процессов этого чата

    char lockpath[64];
    snprintf(lockpath, sizeof(lockpath), "/tmp/shmchat_%d.lock", chat_no);
    lock_fd = open(lockpath, O_CREAT | O_RDWR, 0600);
    if (lock_fd == -1) {
        perror("open lock");
        shmdt(shm);
        return 1;
    }

    // Инициализация сегмента - делает только первый подключившийся

    lock();
    if (shm->seq == 0 && shm->head == 0 && shm->nusers == 0) {
        memset(shm, 0, sz);
    }
    shm->nusers++;
    int users_now = shm->nusers;
    unlock();

    printf("--------------------\n");
    printf("SHM-Chat 0.2\n");
    printf("Chat #%d, user: %s\n", chat_no, me);
    printf("Users online: %d\n", users_now);
    printf("Format: <to> <text>   ('*' = broadcast, '/quit' = exit)\n");
    printf("--------------------\n");
    fflush(stdout);

    // Выходим по ctrl + c
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Разветвляемся: ребёнок — читатель, родитель — писатель.
       Сегмент и lock_fd наследуются через fork. */
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        cleanup();
        return 1;
    }

    if (pid == 0) {
        // Прием сообщений
        for (;;) {
            if (stop) _exit(0);

            lock();
            for (int i = 0; i < N_SLOTS; i++) {
                struct msg *m = &shm->slots[i];
                if (!m->used)              continue;
                if (m->seq <= my_last_seq) continue;
                if (m->seq >  my_last_seq) my_last_seq = m->seq;

                int for_me = (strcmp(m->to, me) == 0) ||
                             (strcmp(m->to, "*") == 0);
                // своё же сообщение не печатаем повторно 
                int from_me = (strcmp(m->from, me) == 0);

                if (for_me && !from_me) {
                    printf("\r[%s -> %s] %s\n> ", m->from, m->to, m->text);
                    fflush(stdout);
                }
            }
            unlock();

            usleep(50 * 1000);
        }
    }

    // отправка сообщений
    char line[512];
    printf("> ");
    fflush(stdout);

    while (!stop && fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "/quit") == 0) break;
        if (line[0] == '\0') { printf("> "); fflush(stdout); continue; }

        char to[NAME_MAX], text[TEXT_MAX];
        if (sscanf(line, "%31s %255[^\n]", to, text) != 2) {
            printf("Format: <to> <text>\n> ");
            fflush(stdout);
            continue;
        }

        lock();
        put_msg_locked(to, text);
        unlock();

        printf("> ");
        fflush(stdout);
    }

    // Завершение: посылаем ребёнку SIGTERM, ждём, чистим
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }

    cleanup();
    return 0;
}