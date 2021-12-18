#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}
sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    if (!head)
    {
        head = tail = timer;
        return;
    }

    //如果新的定时器超时时间小于当前头部结点
    //直接将当前定时器结点作为头部结点
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    //否则调用私有成员，调整内部结点
    add_timer(timer, head);
}

//调整定时器，任务发生变化时，调整定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    util_timer *tmp = timer->next;

    //被调整的定时器在链表尾部
    //或者定时器超时值仍然小于下一个定时器超时值，不调整
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }

    //被调整定时器是链表头结点，将定时器取出，重新插入
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    //被调整定时器在内部，将定时器取出，重新插入
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

//删除定时器
void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }

    //链表中只有一个定时器，需要删除该定时器
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }

    //被删除的定时器为头结点
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    
    //被删除的定时器为尾结点
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }

    //被删除的定时器在链表内部，常规链表结点删除
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

//定时任务处理函数
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    
    //获取当前时间
    time_t cur = time(NULL);
    util_timer *tmp = head;

    //遍历定时器链表
    while (tmp)
    {
        //链表容器为升序排列
        //当前时间小于定时器的超时时间，后面的定时器也没有到期
        if (cur < tmp->expire)
        {
            break;
        }

        //当前定时器到期，则调用回调函数，执行定时事件
        tmp->cb_func(tmp->user_data);

        //将处理后的定时器从链表容器中删除，并重置头结点
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;

    //遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入操作
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    //遍历完发现，目标定时器需要放到尾结点处
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
//仅仅通过管道发送信号值，不处理信号对应的逻辑，缩短异步执行时间，减少对主程序的影响。
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig; //信号值

    //将信号值从管道写端写入，传输字符类型，而非整型
    send(u_pipefd[1], (char *)&msg, 1, 0);

    //将原来的errno赋值为当前的errno
    errno = save_errno;
}

//设置信号函数
//项目中设置信号函数，仅关注SIGTERM和SIGALRM两个信号。
//SIGALRM：由alarm系统调用产生timer时钟信号
//SIGTERM：终端发送的中止信号
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    //创建sigaction结构体变量
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));

    //信号处理函数中仅仅发送信号值，不做对应逻辑处理
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    //将所有信号添加到信号集中
    sigfillset(&sa.sa_mask);

    //执行sigaction函数
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;

//定时器回调函数
//定时事件，具体的，从内核事件表删除事件，关闭文件描述符，释放连接资源
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}

/* 时间轮定时器 */
time_wheel::time_wheel() {
    cur_slot = 0;
    for (int i = 0; i < N; i++) {
        slots[i] = nullptr;
    }
}

time_wheel::~time_wheel() {
    /* 遍历每个槽，并销毁其中的定时器 */
    for (int i = 0; i < N; i++) {
        /* 释放链表上的每个节点 */
        tw_timer *tmp = slots[i];
        while (tmp) {
            slots[i] = tmp->next;
            delete tmp;
            tmp = slots[i];
        }
    }
}

tw_timer *time_wheel::add_timer(int timeout) {
    if (timeout < 0) {
        return nullptr;
    }
    int ticks = 0;
    /* 下面根据带插入定时器的超时值计算它将在时间轮转动多少个滴答后被触发，并将该滴答
     * 数存储于变量ticks中。如果待插入定时器的超时值小于时间轮的槽间隔SI，则将ticks
     * 向上折合为1，否则就将ticks向下折合为timeout/SI */
    if (timeout < SI) {
        ticks = 1;
    } else {
        ticks = timeout / SI;
    }
    /* 计算待插入的定时器在时间轮转动多少圈后被触发 */
    int rotation = ticks / N;
    /* 计算待插入的定时器应该被插入到哪个槽中 */
    int ts = (cur_slot + (ticks % N)) % N;
    /* 创建新的定时器，它在时间轮转动ratation圈之后被触发，且位于第ts个槽上 */
    tw_timer *timer = new tw_timer(rotation, ts, timeout);
    /* 如果第ts个槽中无任何定时器，则把新建的定时器插入其中，并将该定时器设置为该槽的头结点 */
    if (slots[ts] == nullptr) {
        printf("add timer, rotation is %d,cur_slot is %d\n", rotation, ts, cur_slot);
        slots[ts] = timer;
    } else {
        /* 头插法在链表中插入节点 */
        timer->next = slots[ts];
        slots[ts]->prev = timer;

        slots[ts] = timer;
    }
    return timer;
}

void time_wheel::adjust_timer(tw_timer *timer) {
    //删除原本的结点
    if (timer == nullptr) {
        return;
    }
    int ts = timer->time_slot;
    if (timer == slots[ts]) {
        slots[ts] = slots[ts]->next;
        if (slots[ts]) {
            slots[ts]->prev = nullptr;
        }
    } else {
        timer->prev->next = timer->next;
        if (timer->next) {
            timer->next->prev = timer->prev;
        }
    }
    //添加结点
    int ticks = 0;
    if (timer->timeout < SI) {
        ticks = 1;
    } else {
        ticks = timer->timeout / SI;
    }
    /* 计算待插入的定时器在时间轮转动多少圈后被触发 */
    timer->rotation = ticks / N;
    /* 计算待插入的定时器应该被插入到哪个槽中 */
    timer->time_slot = (cur_slot + (ticks % N)) % N;
    ts = timer->time_slot;
    if (slots[ts] == nullptr) {
        slots[ts] = timer;
    } else {
        /* 头插法在链表中插入节点 */
        timer->next = slots[ts];
        slots[ts]->prev = timer;

        slots[ts] = timer;
    }
}

void time_wheel::del_timer(tw_timer *timer) {
    if (timer == nullptr) {
        return;
    }
    int ts = timer->time_slot;
    /* slots[ts]是目标定时器所在槽的头结点。如果目标定时器就是该头结点，则需要
     * 重置第ts个槽的头结点 */
    if (timer == slots[ts]) {
        slots[ts] = slots[ts]->next;
        if (slots[ts]) {
            slots[ts]->prev = nullptr;
        }
        delete timer;
    } else {
        timer->prev->next = timer->next;
        if (timer->next) {
            timer->next->prev = timer->prev;
        }
        delete timer;
    }
}

void time_wheel::tick() {
    tw_timer *tmp = slots[cur_slot];    /* 取得时间轮上当前槽的头结点 */
    printf("current slots is %d\n", cur_slot);
    //循环遍历这个槽上的所有结点
    while (tmp) {
        printf("tick the timer once\n");
        /* 如果定时器的ratation值大于0，则它在这一轮中不起作用 */
        if (tmp->rotation > 0) {
            tmp->rotation--;
            tmp = tmp->next;
        }
            /* 否则说明定时器已经到期，于是执行定时任务，然后删除该定时器 */
        else {
            tmp->cb_func(tmp->user_data);
            if (tmp == slots[cur_slot]) {
                printf("delete header in cur_slot\n");
                slots[cur_slot] = tmp->next;
                delete tmp;
                if (slots[cur_slot]) {
                    slots[cur_slot]->prev = nullptr;
                }
                tmp = slots[cur_slot];
            } else {
                tmp->prev->next = tmp->next;
                if (tmp->next) {
                    tmp->next->prev = tmp->prev;
                }
                tw_timer *tmp2 = tmp->next;
                delete tmp;
                tmp = tmp2;
            }
        }
    }
    /* 更新时间轮的当前槽，以反映时间轮的转动 */
    cur_slot = cur_slot + 1;
    cur_slot = cur_slot % N;
}