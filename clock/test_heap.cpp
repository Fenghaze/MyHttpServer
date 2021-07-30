#include "timeHeap.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char const *argv[])
{
    TimeHeap heaps;
    for (size_t i = 0; i < 5; i++)
    {
        auto node = new HeapNode(i);
        heaps.push(node);
    }

    //延长某个节点的时间
    heaps.heap[0]->expire += 10;
    heaps.output();
    //调整该节点在堆中的位置
    heaps.adjust(heaps.heap[0]);
    heaps.output();

    //删除某个节点
    heaps.del(heaps.heap[4]);
    heaps.output();
    int n = heaps.size();
    for (int i = 0; i < n; i++)
    {
        auto timer = heaps.pop();
        printf("get heaptop timer.expire = %ld\n", timer->expire);
    }

    return 0;
}
