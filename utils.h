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

class TDBManager {
public:
	struct TDBInfo {
		int pid;
		pthread_t tid;
		string nd;
	};

	TDBManager() {
		pthread_mutex_init(&mutex_, NULL);
	}

	~TDBManager() {
		pthread_mutex_destroy(&mutex_);
	}

	bool registerTDB(string name, int pid, pthread_t tid, string nd) {
		lock();
		if (isRegistered(name)) {
			unlock();
			return true;
		}

		TDBInfo info = { pid, tid, nd };
		db_[name] = info;
		bool success = db_.count(name) > 0;
		unlock();

		if (success) {
			cout << "- - - - Server: " << "TDBMS with name " << name
					<< " REGISTRATION SUCCESS" << endl;
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
		cout << "SignalKill will send " << signal << endl;
		lock();
		for (map<string, TDBInfo>::iterator it = db_.begin(); it != db_.end(); it++) {
			cout << "TDB_MANAGER-----> iterating... " << endl;
			if (SignalKill(0, it->second.pid, it->second.tid, signal, SI_USER,
					0) < 0) {
				if (errno == ESRCH) {
					cout << "TDB_MANAGER-----> erase " << it->first << endl;
					db_.erase(it);
				} else {
					cerr << "TDB_MANAGER-----> error SignalKill errno: "
							<< strerror(errno) << endl;
				}
			} else {
				cout << "TDB_MANAGER-----> success signalKill" << endl;
			}
		}

		unlock();

		cout << "TDB_MANAGER-----> TDB_buf iteration end.." << endl;
	}

	void showConnectedTDBMS() {
		cout << "TDB_MANAGER-----> CONNECTED TDB MS:" << endl;
		lock();
		for (map<string, TDBInfo>::iterator it = db_.begin(); it != db_.end(); it++) {
			cout << distance(db_.begin(), it) << ". " << it->first << endl;
		}
		unlock();

	}

	void disconnectTdbms(string name) {
		lock();
		map<string, TDBInfo>::iterator it = db_.find(name);
		if (it != db_.end()) {
			if (SignalKill(0, it->second.pid, it->second.tid, SIG_DISCONNECT_TDBMS, SI_USER, 0) < 0) {
				cerr << "TDB_MANAGER-----> Disconnecting TDBMS: SignalKill error: "<< strerror(errno) << endl;
			}
			db_.erase(it);
			unlock();
			cout << "TDB_MANAGER-----> Successfully removed: " << name << endl;
			return;
		}
		unlock();
		cout << "TDB_MANAGER-----> Not found: " << name << " (can't remove)"
				<< endl;
		return;
	}

private:
	map<string, TDBInfo> db_;
	mutable pthread_mutex_t mutex_;

	void lock() const {
		pthread_mutex_lock(const_cast<pthread_mutex_t*> (&mutex_));
	}

	void unlock() const {
		pthread_mutex_unlock(const_cast<pthread_mutex_t*> (&mutex_));
	}
};

extern TDBManager tdbManager;

#endif /* UTILS_H_ */
