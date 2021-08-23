/**
 * @author: fenghaze
 * @date: 2021/06/08 09:14
 * @desc: 小根堆定时容器
 */

#ifndef TIMEHEAP_H
#define TIMEHEAP_H

#include "../threadpool/HttpServer.h"
#include <deque>
#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define TIMEOUT 1
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

//实现小根堆时间堆：取出小根堆堆顶
class TimeHeap
{
public:
    //初始化堆
    TimeHeap()
    {
        heap.clear();
    }

    TimeHeap(std::deque<HeapNode *> arr)
    {
        if (arr.size())
        {
            for (int i = 0; i < arr.size(); i++)
            {
                heap.push_back(arr[i]);
            }
            //从 (arr.size()-1)/2 开始向下调整小根堆
            for (int i = (arr.size() - 1) / 2; i >= 0; i--)
            {
                siftDown(i, arr.size() - 1);
            }
        }
    }

    ~TimeHeap()
    {
        for (auto element : heap)
        {
            delete element;
        }
    }

    //每插入一个定时器，就向上调整为小根堆
    void push(HeapNode *node)
    {
        if (!node)
        {
            return;
        }
        //插入节点
        heap.push_back(node);
        //printf("size() == %d\n", size());
        //向上调整当前根堆
        int hole = size() - 1;
        int parent = 0;

        /*对从空穴到根节点的路径上的所有节点执行上虑操作*/
        for (; hole > 0; hole = parent)
        {
            parent = (hole - 1) / 2;
            //printf("parent = %d\n", parent);
            if (parent < 0)
            {
                break;
            }
            if (heap[parent]->expire <= node->expire)
            {
                break;
            }
            heap[hole] = heap[parent];
        }
        heap[hole] = node;
    }
    //获得堆顶节点（expire最小），并删除这个顶点，重新调整为小根堆
    HeapNode *pop()
    {
        if (size() == 0)
        {
            return nullptr;
        }
        auto node = heap[0];        //第一个顶点
        heap[0] = heap[size() - 1]; //最后一个顶点放在第一个位置上
        heap.pop_back();            //删除最后一个顶点
        //从顶点开始向下调整为小根堆
        siftDown(0, size() - 1);
        return node; //返回第一个顶点
    }
    //调整指定node所在的根堆
    void adjust(HeapNode *node)
    {
        if (!node)
        {
            return;
        }
        //向下调整以node为根的堆
        int hole = 0;
        for (int i = 0; i < size(); i++)
        {
            if (heap[i] == node)
            {
                hole = i;
                break;
            }
        }
        printf("adjust hole %d\n", hole);
        siftDown(hole, size() - 1);
    }
    //删除指定node，并重新调整根堆
    void del(HeapNode *node)
    {
        for (auto it = heap.begin(); it < heap.end();)
        {
            if (*it == node)
            {
                heap.erase(it);
                it = heap.begin();
            }
            else
            {
                it++;
            }
        }
        siftDown(0, size() - 1);
    }
    int size()
    {
        return heap.size();
    }
    void output()
    {
        for (auto el : heap)
        {
            printf("%ld\t", el->expire);
        }
        printf("\n");
    }

    void tick(int epfd)
    {
        if (heap.empty())
        {
            printf("timer is null\n");
            return;
        }
        printf("timer tick\n");
        auto tmp = pop(); //获得堆顶节点
        time_t cur = time(nullptr);
        while (!heap.empty())
        {
            if (!tmp)
            {
                break;
            }
            if (tmp->expire > cur)
            {
                break;
            }
            if (tmp->callback)
            {
                tmp->callback(epfd, tmp->client);
            }
        }
    }

private:
    //向下调整以hole为根的堆
    void siftDown(int hole, int size)
    {
        auto tmp = heap[hole];
        int child = 0;
        for (; (hole * 2 + 1) <= size; hole = child)
        {
            child = hole * 2 + 1;
            if ((child < size) && (heap[child + 1]->expire < heap[child]->expire))
            {
                child++;
            }
            if (heap[child]->expire < tmp->expire)
            {
                heap[hole] = heap[child];
            }
            else
            {
                break;
            }
        }
        heap[hole] = tmp;
    }

public:
    std::deque<HeapNode *> heap; //堆的存储结构：双端队列
};

#endif // TIMEHEAP_H