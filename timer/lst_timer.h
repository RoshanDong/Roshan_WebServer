#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

#define BUFFER_SIZE 64

//连接资源结构体成员需要用到定时器类
//需要前向声明
class util_timer;
class tw_timer;

//连接资源
struct client_data
{
    //客户端socket地址
    sockaddr_in address;

     //socket文件描述符
    int sockfd;

    //定时器
    tw_timer *timer;
};

//定时器类
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    //超时时间
    time_t expire;
    
    //回调函数
    void (* cb_func)(client_data *);
    //连接资源
    client_data *user_data;
    //前向定时器
    util_timer *prev;
    //后向定时器
    util_timer *next;
};

class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head;
    util_timer *tail;
};


void cb_func(client_data *user_data);

/* 时间轮定时器 */
/* 定时器类 */
class tw_timer {
public:
    tw_timer(int rot, int ts, int tout) {
        next = nullptr;
        prev = nullptr;
        rotation = rot;
        time_slot = ts;
        timeout = tout;  
    }

public:
    int timeout;                        /* 超时时间 */
    int rotation;                       /* 记录定时器在时间轮转多少圈后生效 */
    int time_slot;                      /* 记录定时器属于时间轮上的哪个槽(对应的链表，下同) */
    void (*cb_func)(client_data *);     /* 定时器回调函数 */
    client_data *user_data;             /* 客户端数据 */
    tw_timer *next;                     /* 指向下一个定时器 */
    tw_timer *prev;                     /* 指向前一个定时器 */
};

class time_wheel {
public:
    time_wheel();

    ~time_wheel();

    /* 根据定时值timeout创建一个定时器，并把它插入到合适的槽中 */
    tw_timer *add_timer(int timeout);

    /* 调整目标定时器timer */
    void adjust_timer(tw_timer *timer);

    /* 删除目标定时器timer */
    void del_timer(tw_timer *timer);

    /* SI时间到后，调用该函数，时间轮向前滚动一个槽的间隔 */
    void tick();

private:
    static const int N = 60;    /* 时间轮上槽的数量 */
    static const int SI = 1;    /* 每1 s时间轮转动一次，即槽间隔为1 s */
    tw_timer* slots[N];         /* 时间轮的槽，其中每个元素指向一个定时器链表，链表无序 */
    int cur_slot;               /* 时间轮的当前槽 */
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    time_wheel m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};


#endif
