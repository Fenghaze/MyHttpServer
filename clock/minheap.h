/**
 * @author: fenghaze
 * @date: 2021/08/23 12:48
 * @desc: 
 */

#ifndef MINHEAP_H
#define MINHEAP_H

#include <iostream>
#include <vector>

class HeapNode;

struct client_data
{
    struct sockaddr_in addr;
    int sockfd;
    HeapNode *timer;
};

class HeapNode
{
public:
    HeapNode(int timeout)
    {
        expire = time(nullptr) + timeout; //初始化到期时间
        printf("expire = %ld\n", expire);
    }

public:
    time_t expire;
    client_data *client;
    void (*callback)(int, client_data *);
};

class HeapClock
{
public:
    std::vector<HeapNode *> timers;

public:
    HeapClock()
    {
        timers.clear();
    }
    ~HeapClock()
    {
        timers.clear();
        timers.shrink_to_fit();
    }

public:
    void push(int timeout)
    {
        HeapNode *node = new HeapNode(timeout);
        timers.push_back(node);
        sortHeap();
    }
    void push(HeapNode *node)
    {
        timers.push_back(node);
        sortHeap();
    }
    int getSize()
    {
        return timers.size();
    }

    //调整节点的在根堆中的位置
    void adjust(HeapNode *node, time_t expire_time)
    {
        if (timers.size())
        {
            for (int i = 0; i < timers.size(); i++)
            {
                if (timers[i] == node)
                {
                    timers[i]->expire = expire_time;
                    adjustHeap(timers.size(), i);
                    break;
                }
            }
        }
    }

    //从vector删除指定节点，并重新调整根堆
    void del(HeapNode *node)
    {
        for (auto it = timers.begin(); it < timers.end();)
        {
            if (*it == node)
            {
                timers.erase(it);
                it = timers.begin();
            }
            else
            {
                it++;
            }
        }
        sortHeap();
    }

    //删除小根堆的最小值
    HeapNode *pop()
    {
        if (getSize() == 0)
            return nullptr;
        std::swap(timers[getSize() - 1], timers[0]);
        auto top = timers[getSize() - 1];
        timers.pop_back();
        adjustHeap(getSize(), 0);
        std::cout << "del top " << top->expire << std::endl;
        return top;
    }
    //获得小根堆的顶点
    HeapNode *top()
    {
        if (getSize() == 0)
            return nullptr;
        auto top = timers[0];
        std::cout << "get top " << top->expire << std::endl;
        return top;
    }

    void printTimer()
    {
        std::cout << "heap:";
        for (int i = 0; i < getSize(); i++)
        {
            std::cout << timers[i]->expire << " ";
        }
        std::cout << std::endl;
    }

    void tick(int epfd)
    {
        if (timers.empty())
        {
            //printf("no timer in timers\n");
            return;
        }
        printf("timer tick\n");
        auto tmp = top(); //获得堆顶节点
        time_t cur = time(nullptr);
        if (!tmp)
        {
            return;
        }
        if (tmp->expire > cur)
        {
            printf("unarrvial expire time\n");
            return;
        }
        else if (tmp->callback)
        {
            tmp->callback(epfd, tmp->client);
            pop();
        }
        // while (!timers.empty())
        // {
        //     if (!tmp)
        //     {
        //         break;
        //     }
        //     if (tmp->expire > cur)
        //     {
        //         printf("unarrvial expire time\n");
        //         break;
        //     }
        //     else if (tmp->callback)
        //     {
        //         tmp->callback(epfd, tmp->client);
        //     }
        // }
        
    }

private:
    void sortHeap()
    {
        for (int i = getSize() / 2 - 1; i >= 0; i--)
            adjustHeap(getSize(), i);
    }

    void adjustHeap(int n, int i)
    {
        int j = 2 * i + 1;
        auto temp = timers[i];
        while (j < n)
        {
            if (j + 1 < n && timers[j]->expire > timers[j + 1]->expire)
                j++;
            if (temp->expire < timers[j]->expire)
                break;
            else
            {
                timers[i] = timers[j];
                i = j;
                j = 2 * i + 1;
            }
        }
        timers[i] = temp;
    }
};

#endif // MINHEAP_H