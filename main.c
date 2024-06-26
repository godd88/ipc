#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>


void* f_pipe_w(void* arg)
{
    // printf("[[[ %s_w ]]]: pipe() first then fork(), so don't test\n", (char*)arg);
    return NULL;
}
void* f_pipe_r(void* arg)
{
    printf("[[[ %s_r ]]]: pipe() first then fork(), so don't test\n", (char*)arg);
    return NULL;
}

void* f_fifo_w(void* arg)
{
    int fd = 0;
    time_t tp;
    char buf[1024] = {0};

    if(mkfifo("appfifo", 0666) < 0 && errno!=EEXIST) { // 创建FIFO管道
        perror("fifo: can't open appfifo\n");
        return NULL;
    }
    
    fd = open("appfifo", O_WRONLY); // 单独open 读端或写端会阻塞,直到另一进程打开写或读。可以使用 O_NONBLOCK 避免阻塞
    for (int i=0; i<1; ++i) {
        time(&tp);
        int n = sprintf(buf, "fifo_w: timeis %s", ctime(&tp));  // ctime() 会加\n
        write(fd, buf, n+1);
    }
    close(fd);
    return NULL;
}

void* f_fifo_r(void* arg)
{
    int fd = 0;
    int len = 0;
    char buf[1024] = {0};

    if(mkfifo("appfifo", 0666) < 0 && errno!=EEXIST) { // 创建FIFO管道
        perror("fifo_r: can't open appfifo\n");
        return NULL;
    }

    fd = open("appfifo", O_RDONLY);
    while((len = read(fd, buf, 1024)) > 0) // 读取FIFO管道
        printf("[[[ fifo_r ]]]: %s", buf);

    close(fd);
    return NULL;
}

typedef struct {
    long mtype;  // 第一成员必须是类型
    int a[3];    // 后面的成员无所谓
    char c;
} msq_st;

void* f_msgqueue_w(void* arg)
{
    char* const msqfile = "/etc/passwd";
    key_t key;
    int proj_id = 'z';
    int msqid;
    msq_st msg = {0};

    key = ftok(msqfile, proj_id);
    msqid = msgget(key, IPC_CREAT|0777);

    msg.mtype = 888; // 设置一个 类型
    msg.a[0] = 111;
    msg.a[2] = 333;
    msg.c = 'X';

    msgsnd(msqid, &msg, sizeof(msg.a) + sizeof(msg.c), 0);

    return NULL;
}

void* f_msgqueue_r(void* arg) // 这里关
{
    char* const msqfile = "/etc/passwd";
    key_t key;
    int proj_id = 'z';
    int msqid;
    msq_st msg = {0};

    key = ftok(msqfile, proj_id);
    msqid = msgget(key, IPC_CREAT|0777);

    msgrcv(msqid, &msg, sizeof(msg.a) + sizeof(msg.c), 888, 0);
    printf("[[[ msgqueue_r ]]]: mtype=%ld, a={%d,%d,%d}, c=%c\n", msg.mtype,
                                                         msg.a[0],
                                                         msg.a[1],
                                                         msg.a[2],
                                                         msg.c);
    return NULL;
}

void* f_semaphore_w(void* arg)
{
    char* const semfile = "/etc/passwd";
    key_t key;
    int proj_id = 'z';
    int semid;

    sleep(1); // 等待 f_semaphore_r 初始化, 如果没有初始化 semid集 就V操作，之后P操作是没有资源的

    key = ftok(semfile, 'z');
    semid = semget(key, 1, IPC_CREAT|0666);

    // v操作 释放资源
    struct sembuf sbuf = {0};
    sbuf.sem_num = 0;
    sbuf.sem_op = 1;
    sbuf.sem_flg = SEM_UNDO;
    semop(semid, &sbuf, 1);
    printf("[[[ semaphore_w ]]]: oprate V\n");

    return NULL;
}

void* f_semaphore_r(void* arg)
{
    char* const semfile = "/etc/passwd";
    key_t key;
    int proj_id = 'z';
    int semid;

    key = ftok(semfile, 'z');
    semid = semget(key, 1, IPC_CREAT|0666);

    // init
    semctl(semid, 0, SETVAL, 0);
    // semctl(semid, 0, SETVAL, 1); 信号量集semid 中序号0信号量 初始化成 1, 可以做互斥信号量使用
    printf("[[[ semaphore_r ]]]: init\n");

    // p操作 等待资源
    struct sembuf sbuf = {0};
    sbuf.sem_num = 0; // 信号量序号
    sbuf.sem_op = -1; // p操作
    sbuf.sem_flg = SEM_UNDO;  // 当信号量操作导致进程被阻塞（比如等待信号量为0时），然后进程被信号中断，
                              // 导致操作未完成，此时如果设置了SEM_UNDO标志，
                              // 系统会自动撤销之前对信号量的操作，使得信号量的值回到原来的状态
                              // 简而言之，SEM_UNDO标志用于确保信号量操作的原子性，以避免因为进程意外终止而导致的资源泄漏或者状态异常
    semop(semid, &sbuf, 1); // 操作个数是1，sbf可以是一个数组
    printf("[[[ semaphore_r ]]]: oprate P\n");

    // del_sem
    semctl(semid, 0, IPC_RMID);
    return NULL;
}

void* f_shmem_w(void* arg)
{
    char* const shmfile = "/etc/passwd";
    key_t key;
    int proj_id = 'y';
    int shmid;
    char* shm = NULL;
    int SHMSIZE = 1024;

    key = ftok(shmfile, proj_id);
    shmid = shmget(key, SHMSIZE, IPC_CREAT|0666);

    shm = shmat(shmid, 0, 0);
    sprintf(shm, "aa-bb-cc-dd");

    shmdt(shm);

    return NULL;
}

void* f_shmem_r(void* arg)
{
    char* const shmfile = "/etc/passwd";
    key_t key;
    int proj_id = 'y';
    int shmid;
    char* shm = NULL;
    int SHMSIZE = 1024;

    key = ftok(shmfile, proj_id);
    sleep(1); // 等待 f_shmem_w 给 shm 写字符串
    shmid = shmget(key, SHMSIZE, IPC_CREAT|0666);  // IPC_EXCL|IPC_CREAT|0666 如果存在报错

    shm = shmat(shmid, 0, 0);
    printf("[[[ shmem_r ]]]: %s\n", shm);

    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    return NULL;
}

void* f_mmap_w(void* arg)
{
    size_t len = 20;
    int fd = open("mmap.file", O_RDWR|O_CREAT|O_TRUNC, 0644); // O_RDWR不存在则创建 O_TRUNC截断成0长度
    ftruncate(fd, 20); // 拓展文件大小为 20

    char* p = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    strcpy(p, "AA-BB-CC-DD");
    munmap(p, len);

    return NULL;
}

void* f_mmap_r(void* arg)
{
    size_t len = 20;
    int fd = open("mmap.file", O_RDWR|O_CREAT, 0644);
    sleep(1);

    char* p = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    printf("[[[ mmap_r ]]]: %s\n", p);
    munmap(p, len);

    return NULL;
}

typedef struct {
    void* desc;
    void*(*f_w)(void*);
    void*(*f_r)(void*);
} pcontext;

pcontext pcont[] = {
    {"pipe", f_pipe_w, f_pipe_r},
    {"fifo", f_fifo_w, f_fifo_r},
    {"msg queue", f_msgqueue_w, f_msgqueue_r},    // 传递数据
    {"semaphore", f_semaphore_w, f_semaphore_r},  // 一般仅计数
    {"shmem", f_shmem_w, f_shmem_r},  // 在一块公共内存区建立映射，超高速通信
    {"mmap", f_mmap_w, f_mmap_r},  // 在每个进程中都会开辟一段映射空间，比shmem略低效但是可持续化
};

int main(int argc, char** argv)
{
    int rt = 0;
    int i = 0, pid = 0, ppid = 0;
    pthread_t ctid[255] = {0}, ptid[255];

    ppid = getpid();
    printf("--- partent pid:%d\n", ppid);
    fflush(stdout);


    for (i = 0; i < sizeof(pcont)/sizeof(pcont[0]); ++i) {
        if ((pid = fork()) == 0) {
            printf("--- child pid:%d\n", getpid());
            /* IPC 写 */
            rt = pthread_create(&ctid[i], NULL, pcont[i].f_w, pcont[i].desc);
            break;
        }
    }


    if (!pid) {
        rt = pthread_join(ctid[i], NULL); // 在子进程中等待子线程
    }


    /* IPC 读 */
    if (pid) {
        for ( i = 0; i < sizeof(pcont)/sizeof(pcont[0]); ++i) {
            rt = pthread_create(&ptid[i], NULL, pcont[i].f_r, pcont[i].desc);
        }
    }


    if (pid) {    // 父进程 等待所有子进程退出再退出
        int status;
        for ( i = 0; i < sizeof(pcont)/sizeof(pcont[0]); ++i) {
            wait(&status);
            rt = pthread_join(ptid[i], NULL); // 这里父进程子线程比较多，就按顺序等得了
        }
    }
    printf("%s pid:%d - rt:%d\n", (pid=getpid())==ppid?"--- partent":"--- child", pid, rt);
    return rt;
}
