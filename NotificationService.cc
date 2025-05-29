// NotificationService.cc
#include "NotificationService.h"  // Заголовочный файл сервиса уведомлений
#include "RealTimeService.h"      // Сервис реального времени
#include <sys/neutrino.h>         // Библиотека QNX Neutrino для IPC
#include <time.h>                 // Функции работы со временем

using namespace std;

// Структура для сигнала таймера
struct TimerSignal {
    _pulse pulse;  // Импульсное сообщение QNX
};

// Обработчик сигнала таймера
void handleTimerPulse(const _pulse& pulse) {
    timer_.time++;  // Увеличение счетчика времени

    // Получение текущего времени
    time_t now;
    time(&now);

    // Вывод информации о сигнале
    cout << endl << "Получен сигнал таймера " << ctime(&now);
    cout << "CODE_TIMER = " << (int) pulse.code << endl;
    cout << "Data: " << pulse.value.sival_int << endl;
}

// Основная функция сервиса уведомлений
void* NotificationService::run(void*) {
    cout << "- Notif: запуск канала уведомлений" << endl;

    // Создание канала для приема импульсов таймера
    name_attach_t* attach = name_attach(NULL, NOTIF_CHAN.c_str(), 0);

    if (!attach) {
        cout << "- Notif: ошибка создания канала уведомлений" << endl;
        exit(EXIT_FAILURE);  // Выход при ошибке
    }

    // Ожидание на барьере (синхронизация с таймером)
    pthread_barrier_wait(&notif_barrier);

    // Основной цикл обработки сообщений
    while (!shutDown) {
        cout << endl << "- Notif: ожидание сигнала таймера" << endl;
        TimerSignal tsig;

        // Ожидание сообщения
        int rcvid = MsgReceive(attach->chid, &tsig, sizeof(tsig), NULL);

        // Обработка импульсного сообщения
        if (rcvid == 0) {
            handleTimerPulse(tsig.pulse);  // Обработка импульса таймера

            // Проверка наличия зарегистрированных СУБТД
            if (tdbmsManager.notEmpty()) {
                // Рассылка сигнала всем подписчикам
                tdbmsManager.sendSignalToAll(SIG_SEND_TICK);
            } else {
                cout << "- Notif: Нет зарегистрированных СУБТД..." << endl;
            }
        }
    }

    name_detach(attach, 0);  // Освобождение ресурсов канала
    return NULL;
}
