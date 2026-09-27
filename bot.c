/* автономный бот
   Следит за сообщениями и уведомляет отправителя,
   если адресат недоступен.
   Запуск: ./chatbot <chat_no> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SHM_KEY_BASE 0xDEADBABE
#define N_SLOTS      64
#define NAME_MAX     32
#define TEXT_MAX     256
#define MAX_USERS    32
#define USER_TIMEOUT 30

#define BOT_NAME     "bot"

struct user {
    char   name[NAME_MAX];
    time_t last_seen;
    int    active;
};

struct msg {
    int  used;
    int  seq;
    char from[NAME_MAX];
    char to[NAME_MAX];
    char text[TEXT_MAX];
    int  is_bot_reply;
};

struct chat {
    int          head;
    int          seq;
    int          nusers;
    struct user  users[MAX_USERS];
    struct msg   slots[N_SLOTS];
};

static int          shmid   = -1;
static struct chat *shm     = NULL;
static int          lock_fd = -1;
static int          my_last_seq = 0;
static volatile sig_atomic_t stop = 0;

static void lock(void)   { if (flock(lock_fd, LOCK_EX) == -1) { perror("flock"); exit(1);} }
static void unlock(void) { if (flock(lock_fd, LOCK_UN) == -1) { perror("flock"); exit(1);} }

static void on_signal(int sig) { (void)sig; stop = 1; }

// локальные копии функций, чтобы бот был автономным

static int is_user_available_locked(const char *name)
{
    time_t now = time(NULL);

    if (strcmp(name, "*") == 0) return 1;

    for (int i = 0; i < MAX_USERS; i++) {
        if (shm->users[i].active &&
            strcmp(shm->users[i].name, name) == 0) {
            if (now - shm->users[i].last_seen <= USER_TIMEOUT)
                return 1;
            shm->users[i].active = 0;
            return 0;
        }
    }
    return 0;
}

// записать сообщение от имени бота.
// помечаем is_bot_reply = 1, чтобы бот сам его не обрабатывал

static void bot_reply_locked(const char *to, const char *text)
{
    struct msg *m = &shm->slots[shm->head];
    m->used = 1;
    m->seq  = ++shm->seq;
    strncpy(m->from, BOT_NAME, NAME_MAX - 1); m->from[NAME_MAX - 1] = '\0';
    strncpy(m->to,   to,       NAME_MAX - 1); m->to  [NAME_MAX - 1] = '\0';
    strncpy(m->text, text,     TEXT_MAX - 1); m->text[TEXT_MAX - 1] = '\0';
    m->is_bot_reply = 1;
    shm->head = (shm->head + 1) % N_SLOTS;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <chat_no>\n", argv[0]);
        return 1;
    }
    int chat_no = atoi(argv[1]);
    key_t key = SHM_KEY_BASE + chat_no;

    shmid = shmget(key, sizeof(struct chat), 0666);
    if (shmid == -1) {
        perror("shmget (чат ещё не создан?)");
        return 1;
    }

    shm = (struct chat *) shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        return 1;
    }

    char lockpath[64];
    snprintf(lockpath, sizeof(lockpath), "/tmp/shmchat_%d.lock", chat_no);
    lock_fd = open(lockpath, O_CREAT | O_RDWR, 0600);
    if (lock_fd == -1) {
        perror("open lock");
        shmdt(shm);
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("[bot] started. Watching chat #%d...\n", chat_no);
    fflush(stdout);

    // регистрируем себя как бота
    lock();
    for (int i = 0; i < MAX_USERS; i++) {
        if (shm->users[i].active &&
            strcmp(shm->users[i].name, BOT_NAME) == 0) {
            shm->users[i].last_seen = time(NULL);
            goto registered;
        }
    }
    for (int i = 0; i < MAX_USERS; i++) {
        if (!shm->users[i].active) {
            strncpy(shm->users[i].name, BOT_NAME, NAME_MAX - 1);
            shm->users[i].name[NAME_MAX - 1] = '\0';
            shm->users[i].last_seen = time(NULL);
            shm->users[i].active = 1;
            break;
        }
    }
registered:
    unlock();

    // основной цикл бота 
    while (!stop) {
        lock();

        // обновляем свою метку "жив"
        for (int i = 0; i < MAX_USERS; i++) {
            if (shm->users[i].active &&
                strcmp(shm->users[i].name, BOT_NAME) == 0) {
                shm->users[i].last_seen = time(NULL);
                break;
            }
        }

        // просматриваем новые сообщения 
        for (int i = 0; i < N_SLOTS; i++) {
            struct msg *m = &shm->slots[i];
            if (!m->used)                 continue;
            if (m->seq <= my_last_seq)    continue;

            my_last_seq = m->seq;

            // самого себя не обрабатываем 
            if (m->is_bot_reply)          continue;
            if (strcmp(m->from, BOT_NAME) == 0) continue;


            if (strcmp(m->to, "*") == 0)  continue;

            // если адресат недоступен — отвечаем отправителю 
            if (!is_user_available_locked(m->to)) {
                char reply[TEXT_MAX];
                snprintf(reply, sizeof(reply),
                         "[bot] user '%s' is unavailable "
                         "(not in chat or timed out)", m->to);

                // не отвечаем повторно на одно и то же
                bot_reply_locked(m->from, reply);

                printf("[bot] replied to %s about %s\n", m->from, m->to);
                fflush(stdout);
            }
        }
        unlock();

        usleep(100 * 1000);   // 100 мс 
    }

    // дерегистрация бота 
    lock();
    for (int i = 0; i < MAX_USERS; i++) {
        if (shm->users[i].active &&
            strcmp(shm->users[i].name, BOT_NAME) == 0) {
            shm->users[i].active = 0;
            break;
        }
    }
    unlock();

    shmdt(shm);
    close(lock_fd);
    printf("[bot] stopped.\n");
    return 0;
}