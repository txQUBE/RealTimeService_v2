// RealTimeService.cc
#include "RealTimeService.h"      // Заголовочный файл сервиса реального времени
#include "RegistrationServer.h"   // Сервер регистрации
#include "NotificationService.h"  // Сервис уведомлений
#include <sys/neutrino.h>         // Библиотека QNX Neutrino
#include <stdlib.h>               // Стандартная библиотека
#include <stdio.h>                // Ввод/вывод

// Глобальные переменные
bool shutDown = false;            // Флаг завершения работы
const int CODE_TIMER = 1;         // Код таймера для импульсов
TDBMS_Manager tdbmsManager;       // Менеджер управления СУБТД
pthread_barrier_t notif_barrier;  // Барьер для синхронизации потоков

Timer timer_;                     // Структура таймера

extern const string NOTIF_CHAN;   // Внешнее объявление канала уведомлений

// Установка параметров таймера
void RealTimeService::setTimerProps(struct itimerspec* periodicTimerStruct) {
    periodicTimerStruct->it_value.tv_sec = timer_.tick_sec;        // Установка секунд
    periodicTimerStruct->it_value.tv_nsec = timer_.tick_nsec;      // Установка наносекунд
    periodicTimerStruct->it_interval.tv_sec = timer_.tick_sec;     // Интервал секунд
    periodicTimerStruct->it_interval.tv_nsec = timer_.tick_nsec;   // Интервал наносекунд
}

// Настройка периодического таймера
void RealTimeService::setPeriodicTimer(timer_t* periodicTimer,
        struct itimerspec* periodicTimerStruct, int notif_coid) {
    struct sigevent event;
    // Инициализация события импульса
    SIGEV_PULSE_INIT(&event, notif_coid, SIGEV_PULSE_PRIO_INHERIT, CODE_TIMER, 0);

    // Создание таймера
    if (timer_create(CLOCK_REALTIME, &event, periodicTimer) == -1) {
        perror("Main: error timer_create\n");
        exit(EXIT_FAILURE);  // Выход при ошибке
    }

    setTimerProps(periodicTimerStruct);  // Установка параметров таймера
}

// Настройка и запуск таймера
void RealTimeService::setupTimer() {
    pthread_barrier_wait(&notif_barrier);   // Ожидание на барьере
    pthread_barrier_destroy(&notif_barrier); // Уничтожение барьера

    // Подключение к каналу уведомлений
    int notif_coid = name_open(NOTIF_CHAN.c_str(), 0);
    if (notif_coid == -1) {
        cerr << "main: error name_open(NOTIF_CHAN). errno " << errno
                << endl;
        exit(EXIT_FAILURE);
    }

    // Настройка периодического таймера
    setPeriodicTimer(&timer_.periodicTimer, &timer_.periodicTick, notif_coid);
    timer_settime(timer_.periodicTimer, 0, &timer_.periodicTick, NULL);  // Запуск таймера
    timerId_ = timer_.periodicTimer;  // Сохранение ID таймера

    cout << "#main: Введите 0 чтобы отобразить меню\n";
}

// Запуск потока сервера регистрации
void RealTimeService::startServerThread() {
    if (pthread_create(NULL, NULL, &RegistrationServer::run, NULL) != EOK) {
        perror("main: error server thread launch\n");
    }
}

// Запуск потока сервиса уведомлений
void RealTimeService::startNotificationThread() {
    if (pthread_create(NULL, NULL, &NotificationService::run, NULL) != EOK) {
        perror("main: error notification thread launch\n");
    }
}

// Отображение меню управления
void RealTimeService::showMenu() {
    cout << "-----MENU-----\n";
    cout << "1. Изменить тик таймера\n";
    cout << "2. Показать зарегистрированные СУБТД\n";
    cout << "3. Отключить СУБТД по имени\n";
    cout << "4. Отправить ручной тик\n";
    cout << "999. Завершить работу\n";
    cout << "-----МЕНЮ-----\n";
}

// Обработка пользовательского ввода
void RealTimeService::handleUserInput() {
    while (!shutDown) {
        int userInput;
        cin >> userInput;

        switch (userInput) {
        case 0:
            showMenu();  // Показать меню
            break;
        case 1:
            changeTimerTick();  // Изменить тик таймера
            tdbmsManager.sendSignalToAll(SIG_TIME_DATA_UPDATED);  // Уведомить всех
            break;
        case 2:
            tdbmsManager.showConnectedTDBMS();  // Показать подключенные СУБТД
            break;
        case 3: {
            cout << "Введите имя СУБТД для отключения: \n";
            string tdbNameToDisconnect;
            cin >> tdbNameToDisconnect;
            tdbmsManager.disconnectTdbms(tdbNameToDisconnect);  // Отключить СУБТД
            break;
        }
        case 4:
            cout << "Отправка ручного сигнала...\n";
            tdbmsManager.sendSignalToAll(SIG_SEND_TICK_MANUAL);  // Ручной тик
            break;
        case 999:
            appShutDown();  // Завершение работы
            break;
        }
    }
}

// Изменение параметров таймера
void RealTimeService::changeTimerTick() {
    if (timerId_ == -10) {
        cout << "timer doesn't exist\n";
        return;
    }

    // Ввод новых значений
    cout << "Введите новое значение тика в нсек:\n";
    cin >> timer_.tick_nsec;
    cout << "Введите новое значение тика в сек:\n";
    cin >> timer_.tick_sec;

    // Применение новых параметров
    setTimerProps(&timer_.periodicTick);
    timer_settime(timer_.periodicTimer, 0, &timer_.periodicTick, NULL);

    cout << "Новый тик таймера: " << timer_.tick_sec << " сек "
            << timer_.tick_nsec << " нсек.";
}

// Основной метод работы сервиса
void RealTimeService::run() {
    // Ввод параметров таймера
    cout << "Задать периодичность тика таймера:\nНаносекунд: " << endl;
    cin >> timer_.tick_nsec;
    cout << "Секунд: " << endl;
    cin >> timer_.tick_sec;

    // Инициализация барьера для 2 потоков
    pthread_barrier_init(&notif_barrier, NULL, 2);

    // Запуск серверных потоков
    startServerThread();
    startNotificationThread();

    // Ожидание команды старта
    cout << "Для старта таймера введите '1'" << endl;
    int userInput;
    cin >> userInput;
    while(userInput != 1){
        cin >> userInput;
    }

    setupTimer();  // Настройка таймера
    handleUserInput();  // Обработка пользовательского ввода
}

// Завершение работы приложения
void RealTimeService::appShutDown() {
    shutDown = true;  // Установка флага завершения
}

// Точка входа
int main() {
    RealTimeService service;  // Создание экземпляра сервиса
    service.run();            // Запуск сервиса
    return 0;
}
