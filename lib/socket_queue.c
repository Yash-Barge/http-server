#include <pthread.h>

#define QUEUELEN 5

static int queue_size, q_l, q_r;
static int socket_queue[QUEUELEN];

static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond_full = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_cond_empty = PTHREAD_COND_INITIALIZER;

void enqueue(int socket_fd) {
    pthread_mutex_lock(&queue_lock);

    if (queue_size == QUEUELEN)
        pthread_cond_wait(&queue_cond_full, &queue_lock);

    socket_queue[q_r] = socket_fd;
    queue_size++;
    q_r = (q_r + 1) % QUEUELEN;

    pthread_cond_signal(&queue_cond_empty);
    pthread_mutex_unlock(&queue_lock);

    return;
}

int dequeue(void) {
    pthread_mutex_lock(&queue_lock);

    if (!queue_size)
        pthread_cond_wait(&queue_cond_empty, &queue_lock);

    const int retval = socket_queue[q_l];
    queue_size--;
    q_l = (q_l + 1) % QUEUELEN;

    pthread_cond_signal(&queue_cond_full);
    pthread_mutex_unlock(&queue_lock);

    return retval;
}
