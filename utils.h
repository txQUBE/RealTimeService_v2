// utils.h
#ifndef UTILS_H_
#define UTILS_H_

#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <sys/dispatch.h>
#include <string>
#include <map>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>

using namespace std;

extern bool shutDown;
extern const int CODE_TIMER;
extern pthread_barrier_t notif_barrier;

static const int SIG_SEND_TICK = SIGRTMIN + 1;
static const int SIG_SEND_TICK_MANUAL = SIGRTMIN + 2;
static const int SIG_TIME_DATA_UPDATED = SIGRTMIN + 3;
static const int SIG_DISCONNECT_TDBMS = SIGRTMIN + 9;

class TDBMS_Manager {
public:
	struct TDBMSInfo {
		int pid;
		pthread_t tid;
		string nd;
	};

	TDBMS_Manager() {
		pthread_mutex_init(&mutex_, NULL);
	}

	~TDBMS_Manager() {
		pthread_mutex_destroy(&mutex_);
	}

	bool registerTDBMS(string name, int pid, pthread_t tid, string nd) {
		lock();
		if (isRegistered(name)) {
			unlock();
			return true;
		}

		TDBMSInfo info = { pid, tid, nd };
		db_[name] = info;
		bool success = db_.count(name) > 0;
		unlock();

		if (success) {
			cout << "- - - - Server: " << "СУБТД с именем " << name
					<< "УСПЕШНО ЗАРЕГИСТРИРОВАНА" << endl;
		}
		return success;
	}

	bool isRegistered(string name) const {
		lock();
		bool exists = db_.count(name) > 0;
		unlock();
		return exists;
	}

	bool notEmpty() {
		return !db_.empty();
	}

	void sendSignalToAll(int signal) {
		lock();
		for (map<string, TDBMSInfo>::iterator it = db_.begin(); it != db_.end(); it++) {
			if (SignalKill(0, it->second.pid, it->second.tid, signal, SI_USER,
					0) < 0) {
				if (errno == ESRCH) {
					cout << "TDB_MANAGER-----> отключена СУБТД " << it->first << endl;
					db_.erase(it);
				} else {
					cerr << "TDB_MANAGER-----> ошибка SignalKill, errno: "
							<< strerror(errno) << endl;
				}
			} else {
				cout << "TDB_MANAGER-----> успешный SignalKill" << endl;
			}
		}

		unlock();
	}

	void showConnectedTDBMS() {
		cout << "TDB_MANAGER-----> Зарегистрированые СУБТД:" << endl;
		lock();
		for (map<string, TDBMSInfo>::iterator it = db_.begin(); it != db_.end(); it++) {
			cout << distance(db_.begin(), it) << ". " << it->first << endl;
		}
		unlock();

	}

	void disconnectTdbms(string name) {
		lock();
		map<string, TDBMSInfo>::iterator it = db_.find(name);
		if (it != db_.end()) {
			if (SignalKill(0, it->second.pid, it->second.tid, SIG_DISCONNECT_TDBMS, SI_USER, 0) < 0) {
				cerr << "TDB_MANAGER-----> Ошибка отправки сигнала отключения СУБТД: SignalKill, errno: "<< strerror(errno) << endl;
			}
			db_.erase(it);
			unlock();
			cout << "TDB_MANAGER-----> Успешно отключена СУБТД: " << name << endl;
			return;
		}
		unlock();
		cout << "TDB_MANAGER-----> СУБТД: " << name << " не зарегистрирована (невозможно отключить)"
				<< endl;
		return;
	}

private:
	map<string, TDBMSInfo> db_;
	mutable pthread_mutex_t mutex_;

	void lock() const {
		pthread_mutex_lock(const_cast<pthread_mutex_t*> (&mutex_));
	}

	void unlock() const {
		pthread_mutex_unlock(const_cast<pthread_mutex_t*> (&mutex_));
	}
};

extern TDBMS_Manager tdbmsManager;

#endif /* UTILS_H_ */
