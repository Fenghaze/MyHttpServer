#include "timeHeap.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char const *argv[])
{
    TimeHeap heaps;
    for (size_t i = 0; i < 5; i++)
    {
        auto node = new HeapNode(rand() % 5);
        heaps.push(node);
    }

    heaps.output();
    for (size_t i = 0; i < 5; i++)
    {
        auto timer = heaps.pop();
        printf("timer.expire = %ld\n", timer->expire);
        heaps.output();
    }

    return 0;
}
