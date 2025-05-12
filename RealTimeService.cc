// RealTimeService.cc
#include "RealTimeService.h"
#include "RegistrationServer.h"
#include "NotificationService.h"
#include <sys/neutrino.h>
#include <stdlib.h>
#include <stdio.h>

bool shutDown = false;
const int CODE_TIMER = 1;
TDBManager tdbManager;
pthread_barrier_t notif_barrier;

Timer timer_;

extern const string NOTIF_CHAN;

void RealTimeService::setTimerProps(struct itimerspec* periodicTimerStruct) {
	periodicTimerStruct->it_value.tv_sec = timer_.tick_sec;
	periodicTimerStruct->it_value.tv_nsec = timer_.tick_nsec;
	periodicTimerStruct->it_interval.tv_sec = timer_.tick_sec;
	periodicTimerStruct->it_interval.tv_nsec = timer_.tick_nsec;
}

void RealTimeService::setPeriodicTimer(timer_t* periodicTimer,
		struct itimerspec* periodicTimerStruct, int notif_coid) {
	struct sigevent event;
	SIGEV_PULSE_INIT(&event, notif_coid, SIGEV_PULSE_PRIO_INHERIT, CODE_TIMER, 0);

	if (timer_create(CLOCK_REALTIME, &event, periodicTimer) == -1) {
		perror("Main: error timer_create\n");
		exit(EXIT_FAILURE);
	}

	setTimerProps(periodicTimerStruct);
}

void RealTimeService::setupTimer() {
	pthread_barrier_wait(&notif_barrier);
	pthread_barrier_destroy(&notif_barrier);

	int notif_coid = name_open(NOTIF_CHAN.c_str(), 0);
	if (notif_coid == -1) {
		cerr << "main: error name_open(NOTIF_CHAN). errno " << errno
				<< endl;
		exit(EXIT_FAILURE);
	}

	setPeriodicTimer(&timer_.periodicTimer, &timer_.periodicTick, notif_coid);
	timer_settime(timer_.periodicTimer, 0, &timer_.periodicTick, NULL);
	timerId_ = timer_.periodicTimer;

	cout << "#main: Enter command:\n";
	cout << "#main: Enter 0 to show menu list\n";
}

void RealTimeService::startServerThread() {
	if (pthread_create(NULL, NULL, &RegistrationServer::run, NULL) != EOK) {
		perror("main: error server thread launch\n");
	}
}

void RealTimeService::startNotificationThread() {
	if (pthread_create(NULL, NULL, &NotificationService::run, NULL) != EOK) {
		perror("main: error notification thread launch\n");
	}
}

void RealTimeService::showMenu() {
	cout << "-----MENU-----\n";
	cout << "1. Change timer tick\n";
	cout << "2. Show connected TDB_MS list\n";
	cout << "3. Disconnect TDBMS by name\n";
	cout << "4. Send manual tick signal\n";
	cout << "999. Shut down application\n";
	cout << "-----лемч-----\n";
}

void RealTimeService::handleUserInput() {
	while (!shutDown) {
		int userInput;
		cin >> userInput;

		switch (userInput) {
		case 0:
			showMenu();
			break;
		case 1:
			changeTimerTick();
			tdbManager.sendSignalToAll(SIG_TIME_DATA_UPDATED);
			break;
		case 2:
			tdbManager.showConnectedTDBMS();
			break;
		case 3: {
			cout << "Enter target name to disconnect: \n";
			string tdbNameToDisconnect;
			cin >> tdbNameToDisconnect;
			tdbManager.disconnectTdbms(tdbNameToDisconnect);
			break;
		}
		case 4:
			cout << "Sending manual tik signal...\n";
			tdbManager.sendSignalToAll(SIG_SEND_TICK_MANUAL);
			break;
		case 999:
			appShutDown();
			break;
		}
	}
}

void RealTimeService::changeTimerTick() {
	if (timerId_ == -10) {
		cout << "timer doesn't exist\n";
		return;
	}

	cout << "Enter new Tick value nsec:\n";
	cin >> timer_.tick_nsec;

	cout << "Enter new Tick value sec:\n";
	cin >> timer_.tick_sec;

	setTimerProps(&timer_.periodicTick);
	timer_settime(timer_.periodicTimer, 0, &timer_.periodicTick, NULL);

	cout << "New timer tick: " << timer_.tick_sec << " sec "
			<< timer_.tick_nsec << " nsec.";
}

void RealTimeService::run() {
	timer_.tick_nsec = 0;
	timer_.tick_sec = 5;
	pthread_barrier_init(&notif_barrier, NULL, 2);

	startServerThread();
	startNotificationThread();
	setupTimer();
	handleUserInput();
}

void RealTimeService::appShutDown() {
	shutDown = true;
}

int main() {
	RealTimeService service;
	service.run();
	return 0;
}
