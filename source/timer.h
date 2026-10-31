#pragma once
#include <cstdint>
#include <algorithm>
struct Countdown {
    enum State { Idle, Running, Paused, Finished } state = Idle;
    uint64_t deadline = 0, held = 0;
    void start(uint64_t now, uint64_t duration) { held=duration; deadline=now+duration; state=Running; }
    uint64_t remaining(uint64_t now) const { return state==Running ? (deadline>now ? deadline-now : 0) : held; }
    bool update(uint64_t now) { if(state==Running && now>=deadline){state=Finished;held=0;return true;}return false; }
    void pause(uint64_t now) { if(state==Running){held=remaining(now);state=Paused;} }
    void resume(uint64_t now) { if(state==Paused){deadline=now+held;state=Running;} }
    void reset() {state=Idle;held=deadline=0;}
};
