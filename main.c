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


void* f_pipe_w(void* arg)
{
    printf("%s_w: pipe() first then fork(), so don't test\n", (char*)arg);
    return NULL;
}
void* f_pipe_r(void* arg)
{
    printf("%s_r: pipe() first then fork(), so don't test\n", (char*)arg);
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
    for (int i=0; i<3; ++i) {
        time(&tp);
        int n = sprintf(buf, "fifo_w: timeis %s", ctime(&tp));  // ctime() 会加\n
        write(fd, buf, n+1);
        sleep(1);
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
        printf("fifo_r: %s", buf);

    close(fd);
    return NULL;
}

typedef struct {
    long mtype;  // 第一成员必须是类型
    int a[3];    // 后面的成员无所谓
    char c;
} msq_st;

void* f_msgqueue_r(void*) // 这里关
{
    char* const msqfile = "/etc/passwd";
    key_t key;
    int proj_id = 'z';
    int msqid;
    msq_st msg = {0};

    key = ftok(msqfile, proj_id);
    msqid = msgget(key, IPC_CREAT|0777);

    msgrcv(msqid, &msg, sizeof(msg.a) + sizeof(msg.c), 888, 0);
    printf("msgqueue_r: mtype=%ld, a={%d,%d,%d}, c=%c\n", msg.mtype,
                                                         msg.a[0],
                                                         msg.a[1],
                                                         msg.a[2],
                                                         msg.c);
}

void* f_msgqueue_w(void*)
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

typedef struct {
    void* desc;
    void*(*f_w)(void*);
    void*(*f_r)(void*);
} pcontext;

pcontext pcont[] = {
    {"pipe", f_pipe_w, f_pipe_r},
    {"fifo", f_fifo_w, f_fifo_r},
    {"msg queue", f_msgqueue_w, f_msgqueue_r},
};

int main(int argc, char** argv)
{
    int rt = 0;
    int i = 0, pid = 0, ppid = 0;
    pthread_t ctid[255] = {0}, ptid[255];

    ppid = getpid();
    printf("partent pid:%d\n", ppid);
    fflush(stdout);


    for (i = 0; i < sizeof(pcont)/sizeof(pcont[0]); ++i) {
        if ((pid = fork()) == 0) {
            printf("child pid:%d\n", getpid());
            /* IPC 写 */
            rt = pthread_create(&ctid[i], NULL, pcont[i].f_w, pcont[i].desc);
            break;
        }
    }

    // printf("---- pid:%d ----\n", pid);

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
    printf("%s pid:%d - rt:%d\n", (pid=getpid())==ppid?"partent":"child", pid, rt);
    return rt;
}
