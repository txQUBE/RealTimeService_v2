// RegistrationServer.cc
#include "RegistrationServer.h"  // Заголовочный файл сервера регистрации
#include "RealTimeService.h"     // Заголовочный файл сервиса реального времени
#include "utils.h"               // Вспомогательные утилиты
#include <sys/neutrino.h>        // Библиотека QNX Neutrino для IPC

using namespace std;


// Константы для сервера регистрации
const string REG_CHAN = "RTS_registration_channel";  // Имя канала регистрации
const int REG_TYPE = 101;                            // Тип сообщения регистрации

// Структура сообщения регистрации
struct RegistrationMessage {
    _pulse hdr;         // Заголовок импульса QNX
    char name[255];     // Имя для регистрации
    int pid;            // Идентификатор процесса клиента
    pthread_t tid;      // Идентификатор потока клиента
    char nd[255];       // Дополнительные данные

    long tick_nsec;     // Наносекунды таймера
    int tick_sec;       // Секунды таймера
    int time;           // Время
};

// Обработка ошибок приема сообщений
void handleReceiveError(int error) {
    if (error == ENOTCONN) {
        cout << "- - - - Server: Socket is not connected" << endl;
    } else {
        perror("MsgReceive");  // Вывод системной ошибки
    }
}

// Обработка импульсных сообщений QNX
void handlePulse(const _pulse& pulse) {
    switch (pulse.code) {
    case _PULSE_CODE_DISCONNECT:  // Обработка отключения клиента
        ConnectDetach(pulse.scoid);
        cout << "- - - - Server: _PULSE_CODE_DISCONNECT" << endl;
        break;
    case _PULSE_CODE_UNBLOCK:     // Обработка разблокировки
        cout << "- - - - Server: _PULSE_CODE_UNBLOCK" << endl;
        break;
    default:                      // Обработка других импульсов
        cout << "- - - - Server: получен стандартный импульс" << endl;
        break;
    }
}

// Обработка системных сообщений QNX
bool handleSystemMessage(int rcvid, const _pulse& hdr) {
    if (hdr.type == _IO_CONNECT) {  // Обработка подключения
        MsgReply(rcvid, EOK, NULL, 0);
        return true;
    }

    // Обработка других IO сообщений
    if (hdr.type > _IO_BASE && hdr.type <= _IO_MAX) {
        cout << "_IO_BASE < hdr.type < _IO_MAX" << endl;
        MsgError(rcvid, ENOSYS);  // Отправка ошибки
        return true;
    }

    return false;  // Не системное сообщение
}

// Вспомогательная функция для получения строки из char*
string getStr(const char* str) {
    return string(str);
}

// Обработка сообщения регистрации
void handleRegistrationMessage(int rcvid, const RegistrationMessage& msg) {
    if (msg.hdr.code == REG_TYPE) {  // Проверка типа сообщения
        cout << "- - - - Server: Получен запрос на регистрацию:  " << msg.name
                << endl;

        // Вывод информации о таймере
        cout << " Будут отправлены данные: " << endl;
        cout << "timer_.tick_sec  : " << timer_.tick_sec << endl;
        cout << "timer_.tick_nsec : " << timer_.tick_nsec << endl;
        cout << "timer_.Time      : " << timer_.time << endl;

        // Извлечение данных из сообщения
        string name = getStr(msg.name);
        string nd = getStr(msg.nd);

        // Регистрация в менеджере подключений TDBMS
        bool success = tdbmsManager.registerTDBMS(name, msg.pid, msg.tid, nd);

        if (success) {
            // Подготовка ответного сообщения с данными таймера
            RegistrationMessage reply_msg = msg;
            reply_msg.tick_nsec = timer_.tick_nsec;
            reply_msg.tick_sec = timer_.tick_sec;
            reply_msg.time = timer_.time;

            // Отправка ответа клиенту
            MsgReply(rcvid, EOK, &reply_msg, sizeof(reply_msg));
        } else {
            // Отправка ошибки при неудачной регистрации
            MsgReply(rcvid, EINVAL, NULL, 0);
        }
    }
}

// Основная функция сервера регистрации
void* RegistrationServer::run(void*) {
    cout << "- - - - Server: запуск..." << endl;

    // Создание канала для приема сообщений
    name_attach_t* attach = name_attach(NULL, REG_CHAN.c_str(), 0);

    if (!attach) {
        cerr << "- - - - Server: error name_attach(). errno:" << errno << endl;
        exit(EXIT_FAILURE);  // Выход при ошибке создания канала
    }

    // Основной цикл сервера
    while (!shutDown) {
        cout << "- - - - Server: ожидание запросов..." << endl;
        RegistrationMessage msg;

        // Ожидание сообщения
        int rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1) {
            handleReceiveError(errno);  // Обработка ошибки приема
            continue;
        }

        if (rcvid == 0) {
            handlePulse(msg.hdr);  // Обработка импульса
            continue;
        }

        // Обработка сообщения (системного или регистрации)
        if (!handleSystemMessage(rcvid, msg.hdr)) {
            handleRegistrationMessage(rcvid, msg);
        }
    }

    // Завершение работы сервера
    cout << "- - - - Server: завершение работы" << endl;
    name_detach(attach, 0);  // Освобождение ресурсов канала
    return NULL;
}
