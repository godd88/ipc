#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdatomic.h>

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


#define AUTOMIC_LOOP_TIME 111111
int g_No_Atomic = 0;
//atomic_t g_Atomic = ATOMIC_INIT(0);     // 用于内核开发
atomic_int g_Atomic = ATOMIC_VAR_INIT(0); // 是C11标准, 在C17弃用,在C23中已经删除

void* f_automic_1(void* arg)
{
    for (int i = 0; i < AUTOMIC_LOOP_TIME; i++) {
        ++g_No_Atomic;
        atomic_fetch_add(&g_Atomic, 1);
    }
    return NULL;
}
void* f_automic_2(void* arg)
{
    for (int i = 0; i < AUTOMIC_LOOP_TIME; i++) {
        ++g_No_Atomic;
        atomic_fetch_add(&g_Atomic, 1);
    }
    return NULL;
}

int main(int argc, char** argv)
{
    int rt = 0;
    int i = 0, pid = 0;

    void*(*f_list[])(void*) = {
        f_mutex_1,
        f_mutex_2,
        f_automic_1,
        f_automic_2,
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
    int l_atomic = atomic_load(&g_Atomic);
    LOG_I(g_No_Atomic); // 必然小于 AUTOMIC_LOOP_TIMEx2
    LOG_I(l_atomic);

    LOG_I(rt);
    return rt;
}
