// NotificationService.h
#ifndef NOTIFICATION_SERVICE_H
#define NOTIFICATION_SERVICE_H

#include "utils.h"

const string NOTIF_CHAN = "notification_channel";

class NotificationService {
public:
    static void* run(void*);
};

#endif // NOTIFICATION_SERVICE_H
