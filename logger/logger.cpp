#include "logger.h"
#include <QRegularExpression>


LoggersPool::LoggersPool(int channels) :
    m_logger_channels(channels)
{
    //проверяем количество каналов
    if(m_logger_channels < 0) { m_logger_channels = 0; }
    m_logger_channels = m_logger_channels + 1;
    if(m_logger_channels > MAX_LOGGERS_CHANNELS) {
        m_logger_channels = MAX_LOGGERS_CHANNELS; }

    m_loggers = new Logger*[m_logger_channels];
    for(int i = 0; i < m_logger_channels; i++) {
        m_loggers[i] = new Logger;
    }
    m_timeStartEpochMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
}

LoggersPool::~LoggersPool()
{
    for(int i = 0; i < m_logger_channels; i++) {
        delete m_loggers[i];
    }
    delete [] m_loggers;
}

Logger *LoggersPool::logger(int num)
{
    if(num >= 0 && num < m_logger_channels) {
        return m_loggers[num];
    }
    return m_loggers[0];
}

uint64_t LoggersPool::logtime()
{
    return QDateTime::currentDateTime().toMSecsSinceEpoch() - m_timeStartEpochMs;
}

//LOGGER_STD_STREAM////////////////////////////

LOGGER_STD_STREAM::LOGGER_STD_STREAM()
{
    thread = QThread::currentThread();
    streamLog = new QTextStream(&strBuf);
    streamLog->setEncoding(QStringConverter::Utf8);
    streamLog->setNumberFlags(QTextStream::UppercaseDigits);
}

LOGGER_STD_STREAM::~LOGGER_STD_STREAM()
{
    delete streamLog;
}

//Logger///////////////////////////////////////

Logger::Logger(QObject *parent) : QObject(parent)
{
    // QTextCodec *codec = QTextCodec::codecForName("UTF8");
    // QTextCodec::setCodecForLocale(codec);
    //по умолчанию копирование в stdout/stderr
    copy_to_stderr = copy_to_stdout = true;
    prepend_werror_to_signal = true;
    m_error_blog_now = false;
    logfile_already_opened = false;
    file_append_mode = false;
}

Logger::~Logger()
{
    if(logfile.isOpen()) { logfile.close(); }
    //только так работает, цикл через итератор почему-то слетает
    foreach(LOGGER_STD_STREAM* value, m_mapTextStream) {
        delete value;
    }
}

void Logger::threadDestroyed(QObject* objThread)
{
    //получаем ID потока который прекращает свое существование
    Qt::HANDLE handle = objThread->thread()->currentThreadId();
    //если по странным обстоятельствам этот поток не зарегистрирован
    if(!m_mapTextStream.contains(handle)) { return; }
    //получаем значение регистрационной пары и удаляем ее
    LOGGER_STD_STREAM* value = m_mapTextStream.value(handle);
    m_mapTextStream.remove(handle);
    delete value;
}

QString Logger::fileName()
{
    return logfile.fileName();
}

void Logger::setFileName(QString name)
{
    logfile.setFileName(name);
}

void Logger::slog(const QString &msg)
{
    QWriteLocker locker(&m_lock);
    if(copy_to_stdout) {
        fprintf(stdout, "%s", msg.toUtf8().data()); fflush(stdout);
    }
    emit signalLog(msg);
}

void Logger::serr(const QString &msg)
{
    QWriteLocker locker(&m_lock);
    if(copy_to_stderr) {
        fprintf(stderr, "%s", msg.toUtf8().data()); fflush(stderr);
    }
    if(prepend_werror_to_signal) {
        emit signalErr("error: " + msg);
    }
    emit signalErr(msg);
}

QTextStream& Logger::blog(bool logerr /*= false*/)
{
    m_error_blog_now = logerr;
    //получаем ID потока который начинает это сообщение
    Qt::HANDLE handle = thread()->currentThreadId();
    //если этот поток уже зарегистрирован
    if(m_mapTextStream.contains(handle)) {
        //то отдаем текстовый стрим этого потока
        return *m_mapTextStream.value(handle)->streamLog;
    }
    //этот поток еще не зарегистрирован, создаем пару регистрации
    LOGGER_STD_STREAM *pair = new LOGGER_STD_STREAM;
    //подключимся к сигналу о разрушении этого потока
    connect(thread(), SIGNAL(destroyed(QObject*)), this, SLOT(threadDestroyed(QObject*)));
    m_mapTextStream.insert(handle, pair);
    return *pair->streamLog;
}

void Logger::logend()
{
    Qt::HANDLE handle = QThread::currentThreadId();
    //если по странным обстоятельствам этот поток не зарегистрирован
    if(!m_mapTextStream.contains(handle)) { return; }
    //делаем одну попытку открыть файл логов, если указано имя файла
    if(!logfile_already_opened) {
        if(!logfile.fileName().isEmpty()) {
            if(!logfile.isOpen()) {
                if(file_append_mode) {
                    logfile.open(QIODevice::WriteOnly | QIODevice::Append);
                } else {
                    logfile.open(QIODevice::WriteOnly);
                }
                if(logfile.isOpen()) {
                    slog("file '" + logfile.fileName() + "' is open\n");
                    if(file_append_mode) {
                        logfile.write((QDateTime::currentDateTime().toString("yyyy-MM-dd-HH:mm:ss.zzz") + "\n").toUtf8());
                        logfile.flush();
                    }
                } else {
                    slog("can't open file '" + logfile.fileName() + "'\n");
                }
            }
        }
        logfile_already_opened = true;
    }
    if(logfile.isOpen()) {
        //записываем сообщение в файл
        logfile.write(m_mapTextStream.value(handle)->strBuf.toUtf8());
        logfile.flush();
    }
    //имитируем сообщение и очищаем буфер
    if(m_error_blog_now) {
        serr(m_mapTextStream.value(handle)->strBuf);
    } else {
        slog(m_mapTextStream.value(handle)->strBuf);
    }
    m_mapTextStream.value(handle)->strBuf.clear();
}

void Logger::logrst()
{
    Qt::HANDLE handle = QThread::currentThreadId();
    //если по странным обстоятельствам этот поток не зарегистрирован
    if(!m_mapTextStream.contains(handle)) { return; }
    //ничего никуда не сбрасываем, чистим буфер
    m_mapTextStream.value(handle)->strBuf.clear();
}

//LogTerminal//////////////////////////////////

LogTerminal::LogTerminal(QWidget *parent) : QPlainTextEdit(parent)
{
    int pointSize = 10; setFont(QFont("monospace", pointSize));
    setMaximumBlockCount(4096);
    //авторы Qt так и не определились с размером табуляции (
    //setTabStopWidth(pointSize * 4);
    //setTabStopDistance(pointSize * 4);

    QAction *copyAction = new QAction(tr("&Copy"), this);
    copyAction->setShortcut(Qt::Key_C | Qt::CTRL);
    connect(copyAction, SIGNAL(triggered(bool)), this, SLOT(copy()));
    m_contextMenu = new QMenu("Terminal", this);
    m_contextMenu->addAction(copyAction);

    m_keepEndPosition = true;
    connect(verticalScrollBar(), SIGNAL(sliderMoved(int)), this, SLOT(vertSliderMoved(int)));
}

bool LogTerminal::connectLogCh(int num)
{
    return connect(g_logs.logger(num), SIGNAL(signalLog(QString)), this, SLOT(append(QString)));
}

bool LogTerminal::disconnectLogCh(int num)
{
    return disconnect(g_logs.logger(num), SIGNAL(signalLog(QString)), this, SLOT(append(QString)));
}

void LogTerminal::append(const QString &msg)
{
    //сделаем так, что сдвинутая полоса прокрутки оставалась на месте
    QScrollBar *vertScroll = verticalScrollBar();
    int scrollVal = vertScroll->value();
    int scrollMax = vertScroll->maximum();
    int scrollPage = vertScroll->pageStep();

    //текст вставляем так, чтобы не было лишних переводов строки
    moveCursor(QTextCursor::End);
    insertPlainText(msg);
    moveCursor(QTextCursor::End);

    //если скролл не в конце
    if(!m_keepEndPosition) {
        //если лог заполнен полностью
        if(maximumBlockCount() == scrollPage + scrollMax) {
            //возвращаемся в старую позицию - 1
            vertScroll->setValue(scrollVal - 1);
        } else {
            //возвращаемся в старую позицию
            vertScroll->setValue(scrollVal);
        }
    }
}

void LogTerminal::vertSliderMoved(int pos)
{
    m_keepEndPosition = (pos == verticalScrollBar()->maximum());
}

void LogTerminal::contextMenuEvent(QContextMenuEvent *event)
{
    m_contextMenu->exec(event->globalPos());
}

//DUMPS////////////////////////////////////////

QString log_byte_hex(char byte)
{
    QString str = QString::asprintf("%02X", byte);
    //возвращаем только два последних значения
    return str.right(2);//.toUpper();
}

QString log_bahex_dump(QByteArray ba, uint32_t adr /*= 0*/)
{
    return log_dhex_dump(ba.data(), ba.size(), adr);
}
QString log_shex_dump(const char *str, uint32_t adr /*= 0*/)
{
    return log_dhex_dump(str, strlen(str), adr);
}
QString log_dhex_dump(const char *data, int size, uint32_t adr /* = 0*/)
{
    QString strOut;

    //заглушка на пустой дамп
    bool emptyStub = false;
    if(size == 0) { size = 16; emptyStub = true; }

    //выведем стартовый адрес на 16 битную границу
    uint32_t start_adr = (adr >> 4) << 4;
    uint32_t end_adr = adr + size;
    if((end_adr & 0x0F) == 0) {
        end_adr = (end_adr >> 4) << 4;
    } else {
        end_adr = ((end_adr >> 4) + 1) << 4;
    }
    for(uint32_t i = start_adr; i < end_adr; i++) {
        //печатаем адрес
        if(i % 16 == 0) {
            strOut += QString::asprintf("%04X  ", i);
        }
        //октадный пробел
        if(i % 16 == 8) {
            strOut += " ";
        }
        //значение байта
        int read_adr = i - adr;
        if(read_adr >= 0 && read_adr < size && !emptyStub) {
            strOut += log_byte_hex(data[i - adr]) + " ";
        } else {
            strOut += ".. ";
        }
        //перевод строки
        if(i % 16 == 15 && i != end_adr - 1) {
            strOut += "\n";
        }
    }
    return strOut;
}

int log_int_from_str(QString str, bool *ok, bool hexbydef /*= false*/)
{
    int dgt;

    bool hex = hexbydef;
    if(str.left(1) == "x") {
        str = str.mid(1);
        hex = true;
    }
    if(str.left(2) == "0x") {
        str = str.mid(2);
        hex = true;
    }
    if(hex) {
        dgt = str.toUInt(ok, 16);
    } else {
        dgt = str.toUInt(ok, 10);
    }

    return dgt;
}

bool log_ba_from_str(QString strData, QByteArray *ba, bool hexbydef /*= false*/)
{
    char c; int dgt;
    QString str; bool ok;
    ba->clear();

    //делим входной массив на слова
    QStringList listStr = strData.split(QRegularExpression("\\s+"));
    for(int i = 0; i < listStr.size(); i++) {
        str = listStr[i].trimmed();
        if(str.isEmpty()) { continue; }
        dgt = log_int_from_str(str, &ok, hexbydef);
        if(!ok) { return false; }
        //if(dgt < -128 || dgt > 127) { return false; }
        c = (char)dgt;
        ba->append(c);
    }

    return true;
}