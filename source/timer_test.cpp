#include "timer.h"
#include <cassert>
#include <iostream>
int main(){
    Countdown t;
    t.start(1000,5000);assert(t.remaining(2200)==3800);
    t.pause(2200);assert(t.remaining(999999)==3800);
    t.resume(10000);assert(!t.update(13799));assert(t.update(13800));assert(!t.update(13801));
    assert(t.remaining(99999)==0);
    t.reset();assert(t.state==Countdown::Idle);
    t.start(0,59999999);assert(t.update(60000000));t.reset();assert(!t.update(60000001));
    t.start(0,100);t.pause(99);t.resume(100000);assert(t.remaining(100000)==1);assert(t.update(100001));
    std::cout<<"PASS: pause/resume, exact deadline, completion once, sleep gap, reset, long duration\n";
}
