#ifndef LOCKER_H
#define LOCKER_H

#include <exception> //异常类头文件
#include <pthread.h> //线程头文件
#include <semaphore.h> //信号量头文件 对POSIX信号量来说，信号量是个非负整数。常用于线程间同步。

class sem
{
public:
    sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0) //与其它的函数一样，这些函数在成功时都返回“0”。
        {
            throw std::exception();
        }
    }
    sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        //若sem>0，那么它减1并立即返回。
        //若sem==0，则睡眠直到sem>0，此时立即减1，然后返回。
        return sem_wait(&m_sem) == 0; //sem_wait(或sem_trywait)相当于P操作，即申请资源。 sem_wait是阻塞的，相对的sem_trywait是非阻塞的
    }
    bool post()
    {
        //把指定的信号量sem的值加1;
        //呼醒正在等待该信号量的任意线程。
        return sem_post(&m_sem) == 0; //sem_post相当于V操作，释放资源。
    }

private:
    sem_t m_sem;
};

class locker
{
public:
    locker()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) //初始化互斥锁
        {
            throw std::exception();
        }
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

class cond
{
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0) //初始化条件变量
        {
            //pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex)
    {
        //阻塞在条件变量上
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, m_mutex); //内部会进行一次加锁解锁
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        //阻塞直到指定时间
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool signal()
    {
        //解除在条件变量上的阻塞
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast()
    {
        //释放阻塞的所有线程
        return pthread_cond_broadcast(&m_cond) == 0; //函数以广播的方式唤醒所有等待目标条件变量的线程
    }

private:
    //static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};
#endif
