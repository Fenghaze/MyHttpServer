#include <iostream>
#include <vector>
#include "minheap.h"
using namespace std;


int main(int argc, char const *argv[])
{
    HeapClock clock;
    clock.push(4);
    clock.push(0);
    clock.push(7);
    clock.push(5);
    clock.printTimer();
    auto node = clock.timers[0];
    clock.adjust(node, time(nullptr)+10);
    clock.printTimer();
    auto node2 = clock.timers[2];
    clock.del(node2);
    clock.printTimer();
    return 0;
}
