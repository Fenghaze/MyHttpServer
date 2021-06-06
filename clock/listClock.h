/**
 * @author: fenghaze
 * @date: 2021/06/06 14:42
 * @desc: 升序的双向链表定时器
 */

#ifndef LISTCLOCK_H
#define LISTCLOCK_H

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "../pool/http.h"
#define BUFFERSIZE 2048
#define TIMESLOT 1

class HTTPConn;

//定时器类型（升序链表中的每一个元素）
class ListNode
{
public:
    ListNode() : prev(nullptr), next(nullptr) {}

public:
    time_t expire;                  //任务的超时时间，使用绝对时间
    void (*callback)(int epfd, HTTPConn *); //回调函数处理的客户数据，由定时器的执行者传递给回调函数
    HTTPConn *client_data;
    ListNode *prev; //指向前一个定时器
    ListNode *next; //指向后一个定时器
};

//初始化链表、销毁链表、添加定时事件、调整定时事件、删除定时事件、执行定时任务
class ListClock
{
public:
    ListClock() : head(nullptr), tail(nullptr) {}
    ~ListClock()
    {
        auto tmp = head;
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
    void push(ListNode *node)
    {
        if (!node)
        {
            return;
        }
        if (!head)
        {
            head = node;
            return;
        }
        //头插法：小的事件插入到前面
        if (node->expire < head->expire)
        {
            node->next = head;
            head->prev = node;
            head = node;
            return;
        }
        //往中间插入
        insert(node, head);
    }
    void adjust(ListNode *node)
    {
        if (!node)
        {
            return;
        }
        auto tmp = node->next;

        //timer在末尾 或 timer < tmp，不用调整
        if (!tmp || node->expire < tmp->expire)
        {
            return;
        }

        //node是头节点，则重新插入
        if (node == head)
        {
            head = head->next;
            head->prev = nullptr;
            node->next = nullptr;
            insert(node, head);
        }
        //不是头节点，则删除该节点，并重新插入
        else
        {
            node->prev->next = tmp;
            tmp->prev = node->prev;
            insert(node, tmp);
        }
    }
    void pop(ListNode *node)
    {
        if (!node)
        {
            return;
        }
        //链表中只有一个定时器(链表长度为1)
        if ((node == head) && (node == tail))
        {
            delete node;
            head = nullptr;
            tail = nullptr;
            return;
        }

        //链表中至少有两个定时器，且node==head
        if (node == head)
        {
            head = head->next;
            head->prev = nullptr;
            delete node;
            return;
        }

        //链表中至少有两个定时器，且node==tail
        if (node == tail)
        {
            tail = tail->prev;
            tail->next = nullptr;
            delete node;
            return;
        }

        //node不是head、tail
        node->next->prev = node->prev;
        node->prev->next = node->next;
        delete node;
    }

    //执行定时任务：每隔一段时间调用一次来处理定时事件
    void tick(int epfd)
    {
        if (!head)
        {
            printf("timer is null\n");
            return;
        }
        printf("timer tick\n");
        time_t cur = time(nullptr); //当前时间
        auto tmp = head;
        while (tmp)
        {
            if (cur < tmp->expire) //还没有达到处理时间，break，等待下一次调用tick()
            {
                printf("unarrive expire time");
                break;
            }

            //执行定时回调函数
            tmp->callback(epfd, tmp->client_data);

            //处理完一个定时器就删掉
            head = tmp->next;
            if (head)
            {
                head->prev = nullptr;
            }
            delete tmp;
            tmp = head;
        }
    }

private:
    void insert(ListNode *node, ListNode *lst_head)
    {
        auto tmp = lst_head->next;
        auto pre = lst_head;
        while (tmp)
        {
            if (node->expire < tmp->expire)
            {
                pre->next = node;
                node->next = tmp;
                node->prev = pre;
                tmp->prev = node;
                break;
            }
            pre = tmp;
            tmp = tmp->next;
        }
        //如果tmp < node，则插入到末尾
        if (!tmp)
        {
            pre->next = node;
            node->prev = pre;
            node->next = nullptr;
            tail = node;
        }
        return;
    }

public:
    ListNode *head;
    ListNode *tail;
};

#endif // LISTCLOCK_H