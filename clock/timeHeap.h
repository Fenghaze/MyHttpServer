/**
 * @author: fenghaze
 * @date: 2021/06/08 09:14
 * @desc: 小根堆定时容器
 */

#ifndef TIMEHEAP_H
#define TIMEHEAP_H

#include "../pool/http.h"
#include <deque>

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
    HTTPConn *client_data;
    void (*callback)(int, HTTPConn *);
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
                siftDown(i);
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
        printf("heap.size() == %d\n", heap.size());
        //向上调整当前根堆
        int hole = heap.size() - 1;
        int parent = 0;

        /*对从空穴到根节点的路径上的所有节点执行上虑操作*/
        for (; hole > 0; hole = parent)
        {
            parent = (hole - 1) / 2;
            printf("parent = %d\n", parent);
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
        if (heap.size() == 0)
        {
            return nullptr;
        }
        auto node = heap[0];             //第一个顶点
        heap[0] = heap[heap.size() - 1]; //最后一个顶点放在第一个位置上
        heap.pop_back();                 //删除最后一个顶点
        //从顶点开始向下调整为小根堆
        siftDown(0);
        return node; //返回第一个顶点
    }

    void output()
    {
        for (auto el : heap)
        {
            printf("expire : %ld\t", el->expire);
        }
        printf("\n");
    }

    void tick(int epfd)
    {
        auto tmp = pop();   //获得堆顶节点
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
                tmp->callback(epfd, tmp->client_data);
            }
        }
    }

private:
    //向下调整堆
    void siftDown(int hole)
    {
        auto tmp = heap[hole];
        int child = 0;
        for (; (hole * 2 + 1) <= heap.size() - 1; hole = child)
        {
            child = hole * 2 + 1;
            if ((child < heap.size() - 1) && (heap[child + 1]->expire < heap[child]->expire))
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

private:
    std::deque<HeapNode *> heap; //堆的存储结构：双端队列
};

#endif // TIMEHEAP_H