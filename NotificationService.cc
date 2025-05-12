// NotificationService.cc
#include "NotificationService.h"
#include "RealTimeService.h"
#include <sys/neutrino.h>
#include <time.h>

using namespace std;

struct TimerSignal {
	_pulse pulse;
};

void handleTimerSignal(const _pulse& pulse) {
	timer_.time++;
	time_t now;
	time(&now);

	cout << endl << "Timer signal received " << ctime(&now);
	cout << "CODE_TIMER = " << (int) pulse.code << endl;
	cout << "Data: " << pulse.value.sival_int << endl;
}

void* NotificationService::run(void*) {
	cout << "- Notif: запуск канала уведомлений" << endl;
	name_attach_t* attach = name_attach(NULL, NOTIF_CHAN.c_str(), 0);

	if (!attach) {
		cout << "- Notif: ошибка создания канала уведомлений" << endl;
		exit(EXIT_FAILURE);
	}

	pthread_barrier_wait(&notif_barrier);

	while (true) {
		cout << endl << "- Notif: ожидание сигнала таймера" << endl;
		TimerSignal tsig;
		int rcvid = MsgReceive(attach->chid, &tsig, sizeof(tsig), NULL);

		if (rcvid == 0) {
			handleTimerSignal(tsig.pulse);

			cout
					<< "NOTIF ------> Signal handle success, call sendSignalToAll..."
					<< endl;

			if (tdbManager.notEmpty()) {
				tdbManager.sendSignalToAll(SIG_SEND_TICK);
			} else
				cout << "- Notif: TDB list is empty..." << endl;
		}
	}
	return NULL;
}
