#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define LOG_S(x)  printf("%s:: %s\n", __func__, x)
#define LOG_I(x)  printf("%s:: %s = %d\n", __func__, #x, x)

/*
 * 如果g_count 则打印，加锁之后永远不会打印
 */
#define MUTEX_LOOP_TIME 111111
pthread_mutex_t mutex;
int g_Mutex_Coount = 0;
void* f_mutex_1(void* arg)
{
    for (int i = 0; i < MUTEX_LOOP_TIME; i++) {
        pthread_mutex_lock(&mutex);
        ++g_Mutex_Coount;
        ++g_Mutex_Coount;
        if (g_Mutex_Coount%2) { // 若是 奇数则打印
            LOG_I(g_Mutex_Coount);
        }
        pthread_mutex_unlock(&mutex);
    }
}

void* f_mutex_2(void* arg)
{
    for (int i = 0; i < MUTEX_LOOP_TIME; i++) {
        pthread_mutex_lock(&mutex);
        ++g_Mutex_Coount;
        ++g_Mutex_Coount;
        if (g_Mutex_Coount%2) { // 若是 奇数则打印
            LOG_I(g_Mutex_Coount);
        }
        pthread_mutex_unlock(&mutex);
    }
}

int main(int argc, char** argv)
{
    int rt = 0;
    int i = 0, pid = 0, ppid = 0;

    void*(*f_list[])(void*) = {
        f_mutex_1,
        f_mutex_2
    };
    pthread_t tid[255] = {0};

    pthread_mutex_init(&mutex, NULL);

    for (i = 0; i < sizeof(f_list) / sizeof(f_list[0]); ++i) {
        rt |= pthread_create(&tid[i], NULL, f_list[i], NULL);
    }

    for (i = 0; i < sizeof(f_list) / sizeof(f_list[0]); ++i) {
        rt |= pthread_join(tid[i], NULL); // 防止其他线程结束前进程退出
    }

    pthread_mutex_destroy(&mutex);

    LOG_I(rt);
    return rt;
}
