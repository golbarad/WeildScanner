#include "camerals2.h"

#include <QBitArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QTimer>
#include <QUdpSocket>
#include <QVariant>
#include <QtEndian>
#include <algorithm>
#include <chrono>
#include <cstring>

namespace {
constexpr int SensorWidth = 1280;
constexpr int SensorHeight = 1024;
constexpr int RowHeaderSize = 4;
constexpr int RowPacketSize = RowHeaderSize + SensorWidth;
constexpr int ReceiveBufferSize = 4 * 1024 * 1024;
}

class CameraLS2::Worker : public QObject
{
public:
    explicit Worker(CameraLS2 *camera) : m_camera(camera)
    {
        m_pollTimer->setSingleShot(true);
        m_pollTimer->setTimerType(Qt::PreciseTimer);
        m_timeoutTimer->setSingleShot(true);
        m_timeoutTimer->setTimerType(Qt::PreciseTimer);
        connect(m_pollTimer, &QTimer::timeout, this, &Worker::captureCameraFrame);
        connect(m_timeoutTimer, &QTimer::timeout, this, &Worker::requestTimedOut);
        connect(m_socket, &QUdpSocket::readyRead, this, &Worker::readReplies);
    }

    void start(const QHostAddress &address, int fps, quint16 port, int timeoutMs, quint64 generation)
    {
        stop();
        m_address = address;
        m_port = port;
        m_intervalMs = (1000 + fps - 1) / fps;
        m_timeoutMs = timeoutMs;
        m_generation = generation;
        // Settings register: h_start and h_end (inclusive), two bytes each.
        sendRequest(QByteArray::fromHex("803604"), Request::ReadRows);
    }

    void stop()
    {
        m_pollTimer->stop();
        finishRequest();
    }

private:
    enum class Request { None, ReadRows, Capture };

    void captureCameraFrame()
    {
        try {
            m_image.create(SensorHeight, SensorWidth, CV_8UC1);
        } catch (const cv::Exception &error) {
            postError(QString::fromUtf8(error.what()));
            return;
        }
        m_seenRows.fill(false);
        m_receivedRows = 0;
        m_absoluteRows = true;
        m_relativeRows = true;
        // Measure, wait for completion, and return the image (section 10.3.1).
        sendRequest(QByteArray::fromHex("2da2"), Request::Capture);
    }

    void sendRequest(const QByteArray &command, Request request)
    {
        // Replies have no request ID. A new source port isolates delayed rows
        // from previous requests, including frames that timed out.
        if (!m_socket->bind(QHostAddress::AnyIPv4, 0)) {
            postError(CameraLS2::tr("LS2D: could not bind UDP socket: %1").arg(m_socket->errorString()));
            finishRequest();
            return;
        }
        m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, ReceiveBufferSize);
        if (m_socket->writeDatagram(command, m_address, m_port) != command.size()) {
            postError(CameraLS2::tr("LS2D: could not send UDP request: %1").arg(m_socket->errorString()));
            finishRequest();
            return;
        }
        m_request = request;
        m_requestAge.start();
        m_timeoutTimer->start(m_timeoutMs);
    }

    void readReplies()
    {
        while (m_request != Request::None && m_socket->hasPendingDatagrams()) {
            if (m_requestAge.hasExpired(m_timeoutMs)) {
                requestTimedOut();
                return;
            }

            const auto datagram = m_socket->receiveDatagram(RowPacketSize + 1);
            if (!datagram.isValid()) return;
            if (datagram.senderAddress() != m_address || datagram.senderPort() != m_port) continue;

            const QByteArray data = datagram.data();
            if (m_request == Request::ReadRows) {
                if (data.size() != 5 || quint8(data[0]) != 0x80) continue;
                const int firstRow = qFromBigEndian<quint16>(data.constData() + 1);
                const int lastRow = qFromBigEndian<quint16>(data.constData() + 3);
                finishRequest();
                if (lastRow < firstRow || lastRow >= SensorHeight) {
                    postError(CameraLS2::tr("LS2D: invalid subframe rows %1 to %2").arg(firstRow).arg(lastRow));
                    return;
                }
                m_firstRow = firstRow;
                m_rowCount = lastRow - firstRow + 1;
                m_pollTimer->start(0);
            } else if (data.size() == RowPacketSize) {
                readRow(data.constData());
            }
        }
    }

    void readRow(const char *data)
    {
        const auto frameNumber = qFromBigEndian<quint16>(data);
        const int row = qFromBigEndian<quint16>(data + 2);
        if (m_receivedRows && frameNumber != m_frameNumber) return;

        // The manual does not specify whether row numbers are sensor-relative
        // or subframe-relative. A complete set unambiguously identifies the range.
        const bool absolute = m_absoluteRows && row >= m_firstRow && row < m_firstRow + m_rowCount;
        const bool relative = m_relativeRows && row < m_rowCount;
        if ((!absolute && !relative) || m_seenRows.testBit(row)) return;

        m_absoluteRows = absolute;
        m_relativeRows = relative;
        m_frameNumber = frameNumber;
        m_seenRows.setBit(row);
        std::memcpy(m_image.ptr(row), data + RowHeaderSize, SensorWidth);
        if (++m_receivedRows != m_rowCount) return;

        const int offset = m_absoluteRows ? m_firstRow : 0;
        cv::Mat image = m_image.rowRange(offset, offset + m_rowCount);
        const auto timestampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        finishRequest();
        scheduleNextFrame();
        // Keep at most one frame queued to the owner thread.
        if (m_camera->m_framePending.exchange(true)) return;
        QMetaObject::invokeMethod(m_camera, [camera = m_camera, generation = m_generation, image, timestampNs] {
            camera->m_framePending.store(false);
            if (camera->m_generation == generation) emit camera->frameCaptured(image, timestampNs);
        }, Qt::QueuedConnection);
    }

    void requestTimedOut()
    {
        const bool capturing = m_request == Request::Capture;
        finishRequest();
        if (capturing) {
            postError(CameraLS2::tr("LS2D: frame timed out (%1/%2 rows)").arg(m_receivedRows).arg(m_rowCount));
            scheduleNextFrame();
        } else {
            postError(CameraLS2::tr("LS2D: timed out reading subframe settings"));
        }
    }

    void scheduleNextFrame()
    {
        m_pollTimer->start(int(std::max<qint64>(0, m_intervalMs - m_requestAge.elapsed())));
    }

    void finishRequest()
    {
        m_request = Request::None;
        m_timeoutTimer->stop();
        m_socket->close();
        m_image.release();
    }

    void postError(const QString &error)
    {
        QMetaObject::invokeMethod(m_camera, [camera = m_camera, generation = m_generation, error] {
            if (camera->m_generation == generation) emit camera->captureError(error);
        }, Qt::QueuedConnection);
    }

    CameraLS2 *const m_camera;
    QUdpSocket *const m_socket = new QUdpSocket(this);
    QTimer *const m_pollTimer = new QTimer(this);
    QTimer *const m_timeoutTimer = new QTimer(this);
    QHostAddress m_address;
    quint16 m_port = 0;
    int m_intervalMs = 0;
    int m_timeoutMs = 0;
    quint64 m_generation = 0;
    Request m_request = Request::None;
    QElapsedTimer m_requestAge;
    int m_firstRow = 0;
    int m_rowCount = 0;
    quint16 m_frameNumber = 0;
    int m_receivedRows = 0;
    bool m_absoluteRows = true;
    bool m_relativeRows = true;
    QBitArray m_seenRows = QBitArray(SensorHeight);
    cv::Mat m_image;
};

CameraLS2::CameraLS2(QObject *parent) : ICamera(parent), m_worker(new Worker(this))
{
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();
}

CameraLS2::~CameraLS2()
{
    m_thread.quit();
    m_thread.wait();
}

ICamera::OperationResult CameraLS2::start(const Source &source)
{
    if (QThread::currentThread() != thread()) return OperationResult::WrongThread;
    const auto *network = std::get_if<Network>(&source);
    if (!network) return OperationResult::UnsupportedSource;

    const QHostAddress address(network->address);
    if (address.protocol() != QAbstractSocket::IPv4Protocol || address == QHostAddress::AnyIPv4
        || address == QHostAddress::Broadcast || address.isMulticast()
        || network->fps < 1 || network->fps > 1000 || network->port == 0 || network->timeoutMs < 1) {
        emit captureError(tr("LS2D: invalid IPv4 address, UDP port, frame rate or timeout"));
        return OperationResult::Failed;
    }

    const bool queued = QMetaObject::invokeMethod(m_worker,
        [worker = m_worker, address, config = *network, generation = ++m_generation] {
            worker->start(address, config.fps, config.port, config.timeoutMs, generation);
        }, Qt::QueuedConnection);
    return queued ? OperationResult::Pending : OperationResult::Failed;
}

ICamera::OperationResult CameraLS2::stopCamera()
{
    if (QThread::currentThread() != thread()) return OperationResult::WrongThread;
    ++m_generation;
    const bool queued = QMetaObject::invokeMethod(m_worker, [worker = m_worker] { worker->stop(); }, Qt::QueuedConnection);
    return queued ? OperationResult::Pending : OperationResult::Failed;
}
