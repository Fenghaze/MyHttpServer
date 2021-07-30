/**
 * @author: fenghaze
 * @date: 2021/06/07 09:22
 * @desc: 实现时间轮
 */

#ifndef TIMEWHEEL_H
#define TIMEWHEEL_H

#include "../pool/http.h"
#include <vector>
//时间轮的槽链表头节点
class WheelNode
{
public:
    int rot;                                //在时间轮中旋转的次数，达到某个次数时，执行定时事件，相当于expire
    int slot;                               //所在的槽
    void (*callback)(int epfd, HTTPConn *); //回调函数
    HTTPConn *client_data;
    WheelNode *prev; //链表的前一个节点
    WheelNode *next; //链表的后一个节点
public:
    WheelNode(int rotate, int slot_idx) : rot(rotate), slot(slot_idx), prev(nullptr), next(nullptr) {}
};

//时间轮
class TimeWheel
{
public:
    TimeWheel() : cur_slot(0)
    {
        slots.clear();
    }
    ~TimeWheel()
    {
        for (auto slot : slots)
        {
            delete slot;
        }
    }

    //插入：根据timeout创建一个定时器，并插入到合适的slot中
    void push(int timeout)
    {
        if (timeout < 0)
        {
            return;
        }
        int ticks = 0; //当前timeout是SI的几倍
        if (SI > timeout)
        {
            ticks = 1;
        }
        else
        {
            ticks = timeout / SI;
        }
        //计算圈数
        int rotate = ticks / N;
        //计算最后所在的slot下标
        int slot_idx = (cur_slot + ticks % N) % N;

        //创建链表节点：当前定时事件在时间轮的第rotate圈，第slot_idx个slot执行
        auto node = new WheelNode(rotate, slot_idx);
        //如果当前slot为空，则作为头节点
        if (!slots[slot_idx])
        {
            printf("add timer, rotate%d, slot_idx=%d, cur_slot=%d\n", rotate, slot_idx, cur_slot);
            slots[slot_idx] = node;
        }
        //否则使用头插法插入
        else
        {
            node->next = slots[slot_idx];
            slots[slot_idx]->prev = node;
            slots[slot_idx] = node;
        }
        return;
    }

    //删除定时事件
    void pop(WheelNode *node)
    {
        if (!node)
        {
            return;
        }
        //所在的索引
        int slot_idx = node->slot;
        //如果是头节点
        if (node == slots[slot_idx])
        {
            slots[slot_idx] = slots[slot_idx]->next;
            if (slots[slot_idx])
            {
                slots[slot_idx]->prev = nullptr;
            }
            delete node;
        }
        else
        {
            node->prev->next = node->next;
            if (node->next)
            {
                node->next->prev = node->prev;
            }
            delete node;
        }
    }

    //执行定时事件
    void tick(int epfd)
    {
        //获取当前槽的头节点
        auto tmp = slots[cur_slot];
        while (tmp)
        {
            printf("tick once\n");
            if (tmp->rot > 0)
            {
                tmp->rot--;
                tmp = tmp->next;
            }
            //rot==0，说明定时器到期，执行定时回调函数
            else
            {
                tmp->callback(epfd, tmp->client_data);
                //如果是头节点，则删除头节点
                if (tmp == slots[cur_slot])
                {
                    printf("delete header in cur_slot\n");
                    slots[cur_slot] = tmp->next;
                    delete tmp;
                    if (slots[cur_slot])
                    {
                        slots[cur_slot]->prev = nullptr;
                    }
                }
                //如果是中间的节点，则删除中间节点
                else
                {
                    tmp->prev->next = tmp->next;
                    if (tmp->next)
                    {
                        tmp->next->prev = tmp->prev;
                    }
                    auto tmp2 = tmp->next;
                    delete tmp;
                    tmp = tmp2;
                }
            }
        }
        //更新时间轮当前槽的索引，表示轮子在转动
        cur_slot = ++cur_slot % N;
    }

public:
    static const int N = 60;        //时间轮槽的总数
    static const in SI = 1;         //槽间隔为1s
    std::vector<WheelNode *> slots; //时间轮数据结构
    int cur_slot;                   //当前槽的索引
};
}

#endif // TIMEWHEEL_H