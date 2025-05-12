// RealTimeService.h
#ifndef REALTIME_SERVICE_H
#define REALTIME_SERVICE_H

#include "utils.h"

using namespace std;

struct Timer {
    long tick_nsec;
    int tick_sec;
    int time;
    timer_t periodicTimer;
    struct itimerspec periodicTick;
};

extern Timer timer_;

class RealTimeService {
public:
    void run();

private:
    timer_t timerId_;

    void setTimerProps(struct itimerspec* periodicTimerStruct);
    void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int notif_coid);
    void setupTimer();
    void startServerThread();
    void startNotificationThread();
    void showMenu();
    void changeTimerTick();
    void handleUserInput();
    void appShutDown();
};

#endif // REALTIME_SERVICE_H
