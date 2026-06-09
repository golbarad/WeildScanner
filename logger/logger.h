#ifndef LOGGER_H
#define LOGGER_H

/*
    НАБОР ИНСТРУМЕНТАРИЯ СИСТЕМЫ ЛОГОВ ПРИЛОЖЕНИЯ
    версия от 2023-07-31
    Исходники: /home/jury/start_mp/src/logger/logger.h/cpp

    Создан по-чуть-чуть из всяких классов Qt и удобен для написания
    технических приложений. Имеется система макросов и развязка потоков.

    Для использования этой системы логирования, нужно где-то в
    программе вызвать макрос с кол-вом каналов (см. код и правило +1 ch)
        LOGGER_ALLOC(nc)
    Будет создан глобальный объект с уникальным именем
        g_logs
    Замечание: если в коде где-то есть
        #include "logger.h"
    то обязательно нужно один раз где-то вызвать LOGGER_ALLOC,
    отсутствие этого вызова приводит к ошибке:
        ошибка: undefined reference to `g_logs'
    и повторный вызов приводит к ошибке
        ошибка: redefinition of ‘LoggersPool g_logs’

    ИСПОЛЬЗОВАНИЕ МАКРОСОВ ЛОГИРОВАНИЯ
    Для получения вермени в миллисекундах от старта системы логов
        LTIME

    Для стиля std для русского текста оябазтельно использовать TR
        BLOG(0) << TR("Это русский текст") << ENDL; LOGEND(0);
    Можно вести несколько логов одновременно и с разрывами
        BLOG(0) << TR("Начат лог ") << 0 << TR("-го канала, ");
        BLOG(1) << TR("Начат лог ") << 1 << TR("-го канала, ");
        BLOG(0) << TR("продолжаем для ") << 0 << TR("-го канала") << ENDL;
        BLOG(1) << TR("продолжаем для ") << 1 << TR("-го канала") << ENDL;
        LOGEND(0); LOGEND(1);
        BLOG(0) << TR("Еще строка") << ENDL; LOGEND(0);
    сброс накопленного сообщения в канал происходит после вызова LOGEND(0)

    Макрос вызова функции создания форматированой QString::asprintf()
    строки FSTR сильно сокращает объем кода и более удобно чем std
        BLOG(0) << FSTR("num:%04i\n", 111); LOGEND(0);

    АДАПТАЦИЯ ТРАДИЦИОНОГО КОДА
    Заменить типичные записи логов stdio:
        sprintf(stdout,         -->     SLOG(0,             без форматирования
        sprintf(stderr,         -->     SERR(0,             без форматирования
        sprintf(stdout, "...    -->     SLOG(0, FSTR()      с форматированием
        sprintf(stderr, "...    -->     SERR(0, FSTR()      с форматированием
        printf(                 -->     SLOG(0,             без форматирования
        printf("...             -->     SLOG(0, FSTR()      с форматированием
    убрать fflush(), т.к. добавляется автоматически
    Заменить типичные записи логов std:
        std::cout << ... ;      -->     BLOG(0) << ... LOGEND(n);
        std::cerr << ... ;      -->     BERR(0) << ... LOGEND(n);
    для совместимости заменить endl на ENDL

    ИСПОЛЬЗОВАНИЕ ТЕРМИНАЛОВ
    очень простое, достаточно создать один или несколько LogTerminal
    и подключить каналы логера:
        LogTerminal *terminal = new LogTerminal();
        terminal->setWindowTitle("TERMINAL 0");
        terminal->connectLogCh(0);
        terminal->connectLogCh(1);
        terminal->show();
    Каналы можно подключать или отключать динамически, например, из меню
*/

#include <QObject>
#include <QThread>
#include <QTextStream>
//#include <QTextC>
#include <QReadWriteLock>
#include <QMap>
#include <QFile>
#include <QDateTime>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QMenu>
#include <QContextMenuEvent>

struct LoggersPool;
extern LoggersPool g_logs;

#define MAX_LOGGERS_CHANNELS    64


//макросы упрощающие жизнь программиста
#define LOGGER_ALLOC(nc)        LoggersPool g_logs(nc);
#define LTIME                   g_logs.logtime()
#define TR(rus)                 QObject::tr(rus)
#define FSTR                    QString::asprintf
#define BAHEX(d)                log_bahex_dump(d)
#define BAHEXA(d, a)            log_bahex_dump(d, a)
#define DHEX(d, s)              log_dhex_dump(d, s)
#define DHEXA(d, s, a)          log_dhex_dump(d, s, a)
#define LOGGER(n)               g_logs.logger(n)
#define SLOG(n, s)              g_logs.logger(n)->slog(s)
#define SERR(n, s)              g_logs.logger(n)->serr(s);
#define BLOG(n);                g_logs.logger(n)->blog()
#define BERR(n)                 g_logs.logger(n)->berr()
#define LOGEND(n)               g_logs.logger(n)->logend()
#define LOGRST(n)               g_logs.logger(n)->logrst()

class Logger;

//структура пула каналов логирования
class LoggersPool {
public:
    LoggersPool(int channels);
    ~LoggersPool();
    //
    //получить канал логирования
    Logger *logger(int num);
    //получить время в миллисекундах
    uint64_t logtime();

protected:
    //каналы логирования 0 - default, 1, 2, 3, ..
    Logger **m_loggers;
    //количество каналов логирования
    int m_logger_channels;
    //время старта логирования в миллисекундах
    uint64_t m_timeStartEpochMs;
};

//cтруктура для карты "каждому потоку свой поток"
struct LOGGER_STD_STREAM {
    LOGGER_STD_STREAM();
    ~LOGGER_STD_STREAM();
    //
    //указатель на поток формирующий сообщение
    QThread *thread;
    //накапливаемая текстовая строка
    QString strBuf;
    //текстовый стрим для m_strBuf
    QTextStream *streamLog;
};

//класс формирования логов
class Logger : public QObject
{
    Q_OBJECT
public:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();

public:
    //имя канала (внутри не используется)
    QString name;
    //имя файла (автоматическое открытие на первом логе)
    QString fileName(); void setFileName(QString name);
    bool file_append_mode; //файл а режиме добавления
    //дублировать сообщения в stdout
    bool copy_to_stdout;
    //дублировать сообщения в stderr
    bool copy_to_stderr;
    //сигнал ошибки предворять словом error
    bool prepend_werror_to_signal;

    //отправка сообщения в стиле stdio
    void slog(const QString &msg);
    void serr(const QString &msg);

    //начало сообщения в стиле std
    QTextStream& blog(bool logerr = false);
    QTextStream& berr() { return blog(true); }
    //окончание сообщения в стиле std
    void logend();
    //сбросить накопленный лог
    void logrst();

signals:
    //сигналы от глобального объекта
    void signalLog(const QString &msg);
    void signalErr(const QString &msg);

protected slots:
    //зарегистрированный поток разрушается
    void threadDestroyed(QObject* objThread);

protected:
    QFile logfile; bool logfile_already_opened;
    //сейчас собирается сообщение ошибки
    bool m_error_blog_now;
    //разделение доступа из разных потоков
    QReadWriteLock m_lock;
    //карта буферов сообщений и потоков для стиля std
    QMap<Qt::HANDLE, LOGGER_STD_STREAM*> m_mapTextStream;
};

//класс редактора адаптированного под терминал
class LogTerminal : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit LogTerminal(QWidget *parent = nullptr);

    //подключить к терминалу канал логера
    bool connectLogCh(int num);
    //отключить от терминала канал логера
    bool disconnectLogCh(int num);

public slots:
    //добавить сообщение
    void append(const QString &msg);

protected slots:
    //пользователь отпустил вертикальный скролл
    virtual void vertSliderMoved(int pos);
    //чтобы иметь курсор для копирования из терминала нужно не ReadOnly
    /*//можно просто игноририровать события редактирования текста
    virtual void keyPressEvent(QKeyEvent *e) { e->ignore(); }
    virtual void keyReleaseEvent(QKeyEvent *e) { e->ignore(); }
    virtual void dragEnterEvent(QDragEnterEvent *e) { e->ignore(); }
    virtual void dragLeaveEvent(QDragLeaveEvent *e) { e->ignore(); }
    virtual void dragMoveEvent(QDragMoveEvent *e) { e->ignore(); }
    virtual void dropEvent(QDropEvent *e) { e->ignore(); }
    //... а можно оставить редактирование - так как особо и не мешает*/
    //контекстное меню имеет только одно копирование
    virtual void contextMenuEvent(QContextMenuEvent *e);

protected:
    QMenu *m_contextMenu;
    //хранить последнюю позицию
    bool m_keepEndPosition;
};

/*
    Глобальные и Полезные функции формирования Дампингов
*/

//если распечатать asprintf("%02X", -1), то получим FFFFFFFF - казуслибы
//эта функция исправляет эту проблему, оставляя только 2 последних символа
QString log_byte_hex(char byte);

//распечатать дамп памяти, если указать начальный адрес, то текст дампа
//будет начинаться с указанного адреса и смещен по 16-ти битной границе
QString log_bahex_dump(QByteArray ba, uint32_t adr = 0);
QString log_shex_dump(const char *str, uint32_t adr = 0);
QString log_dhex_dump(const char *data, int size, uint32_t adr = 0);

//получить число из строки, если hexbydef = true, то
//всегда 16-ричная интерпретация, если hexbydef = false,
//то основание определяется значением префикса '0x' или 'x'
int log_int_from_str(QString str, bool *ok, bool hexbydef = false);

//получить дамп из текстовых цифр разделенных пробелами
//в исходной строке могут быть ошибки, поэтому см. return
bool log_ba_from_str(QString str, QByteArray *ba, bool hexbydef = false);

#endif // LOGGER_H
