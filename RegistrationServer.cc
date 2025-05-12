// RegistrationServer.cc
#include "RegistrationServer.h"
#include "RealTimeService.h"
#include "utils.h"
#include <sys/neutrino.h>

using namespace std;

namespace {
const string REG_CHAN = "RTS_registration_channel";
const int REG_TYPE = 101;

//#pragma pack(push, 1)
struct RegistrationMessage {
	_pulse hdr;
	char name[255];
	int pid;
	pthread_t tid;
	char nd[255];

	long tick_nsec;
	int tick_sec;
	int time;
};
//#pragma pack(pop)

void handleReceiveError(int error) {
	if (error == ENOTCONN) {
		cout << "- - - - Server: Socket is not connected" << endl;
	} else {
		perror("MsgReceive");
	}
}

void handlePulse(const _pulse& pulse) {
	switch (pulse.code) {
	case _PULSE_CODE_DISCONNECT:
		ConnectDetach(pulse.scoid);
		cout << "- - - - Server: _PULSE_CODE_DISCONNECT" << endl;
		break;
	case _PULSE_CODE_UNBLOCK:
		cout << "- - - - Server: _PULSE_CODE_UNBLOCK" << endl;
		break;
	default:
		cout << "- - - - Server: default pulse" << endl;
		break;
	}
}

bool handleSystemMessage(int rcvid, const _pulse& hdr) {
	if (hdr.type == _IO_CONNECT) {
		MsgReply(rcvid, EOK, NULL, 0);
		return true;
	}

	if (hdr.type > _IO_BASE && hdr.type <= _IO_MAX) {
		cout << "_IO_BASE < hdr.type < _IO_MAX" << endl;
		MsgError(rcvid, ENOSYS);
		return true;
	}

	return false;
}

string getStr(const char* str) {
	return string(str);
}

void handleRegistrationMessage(int rcvid, const RegistrationMessage& msg) {
	if (msg.hdr.code == REG_TYPE) {
		cout << "- - - - Server: call tdbManager.registerTDB for " << msg.name
				<< endl;

		cout << " TIMER DATA TO SEND : " << endl;
		cout << "timer_.tick_sec  : " << timer_.tick_sec << endl;
		cout << "timer_.tick_nsec : " << timer_.tick_nsec << endl;
		cout << "timer_.Time      : " << timer_.time << endl;

		string name = getStr(msg.name);
		string nd = getStr(msg.nd);
		bool success = tdbManager.registerTDB(name, msg.pid, msg.tid, nd);

		if (success) {
			RegistrationMessage reply_msg = msg;
			reply_msg.tick_nsec = timer_.tick_nsec;
			reply_msg.tick_sec = timer_.tick_sec;
			reply_msg.time = timer_.time;

			MsgReply(rcvid, EOK, &reply_msg, sizeof(reply_msg));
		} else {
			MsgReply(rcvid, EINVAL, NULL, 0);
		}

	}
}
} // namespace

void* RegistrationServer::run(void*) {
	cout << "- - - - Server: starting..." << endl;
	name_attach_t* attach = name_attach(NULL, REG_CHAN.c_str(), 0);

	if (!attach) {
		cerr << "- - - - Server: error name_attach(). errno:" << errno << endl;
		exit(EXIT_FAILURE);
	}

	while (!shutDown) {
		cout << "- - - - Server: wait for msg..." << endl;
		RegistrationMessage msg;
		int rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);

		if (rcvid == -1) {
			handleReceiveError(errno);
			continue;
		}

		if (rcvid == 0) {
			handlePulse(msg.hdr);
			continue;
		}

		if (!handleSystemMessage(rcvid, msg.hdr)) {
			handleRegistrationMessage(rcvid, msg);
		}
	}

	cout << "- - - - Server: Server is shutting down" << endl;
	name_detach(attach, 0);
	return NULL;
}
