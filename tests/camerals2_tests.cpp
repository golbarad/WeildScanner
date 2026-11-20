#include "apcamera/camerals2.h"

#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QTest>
#include <QUdpSocket>
#include <QtEndian>
#include <algorithm>
#include <memory>

namespace {
const QByteArray ReadRows = QByteArray::fromHex("803604");
const QByteArray Capture = QByteArray::fromHex("2da2");
const QByteArray RowSettings = QByteArray::fromHex("8000020005");

QByteArray rowPacket(quint16 frameNumber, quint16 row, char brightness)
{
    QByteArray packet(1284, brightness);
    qToBigEndian(frameNumber, packet.data());
    qToBigEndian(row, packet.data() + 2);
    return packet;
}

void reply(QUdpSocket &socket, const QNetworkDatagram &request, const QByteArray &data)
{
    QCOMPARE(socket.writeDatagram(data, request.senderAddress(), request.senderPort()), qint64(data.size()));
}
}

class CameraLS2Tests : public QObject
{
    Q_OBJECT
private slots:
    void rejectsUnsupportedSource()
    {
        std::unique_ptr<ICamera> camera = std::make_unique<CameraLS2>();
        QCOMPARE(camera->start(ICamera::Synthetic{}), ICamera::OperationResult::UnsupportedSource);
    }

    void reassemblesFrame_data()
    {
        QTest::addColumn<int>("rowOffset");
        QTest::newRow("sensor rows") << 2;
        QTest::newRow("subframe rows") << 0;
    }

    void reassemblesFrame()
    {
        QFETCH(int, rowOffset);
        QUdpSocket sensor;
        QUdpSocket foreign;
        QVERIFY(sensor.bind(QHostAddress::LocalHost, 0));
        QVERIFY(foreign.bind(QHostAddress::LocalHost, 0));
        connect(&sensor, &QUdpSocket::readyRead, this, [&] {
            while (sensor.hasPendingDatagrams()) {
                const auto request = sensor.receiveDatagram();
                if (request.data() == ReadRows) {
                    reply(sensor, request, RowSettings);
                    continue;
                }
                QCOMPARE(request.data(), Capture);
                reply(foreign, request, rowPacket(99, rowOffset + 1, 99));
                reply(sensor, request, rowPacket(99, rowOffset + 1, 99) + '\0');
                reply(sensor, request, rowPacket(7, rowOffset + 1, 11));
                reply(sensor, request, rowPacket(7, rowOffset + 1, 99));
                reply(sensor, request, rowPacket(8, rowOffset + 2, 99));
                reply(sensor, request, rowPacket(7, rowOffset + 3, 13));
                reply(sensor, request, rowPacket(7, rowOffset + 0, 10));
                reply(sensor, request, rowPacket(7, rowOffset + 2, 12));
            }
        });

        std::unique_ptr<ICamera> camera = std::make_unique<CameraLS2>();
        QSignalSpy frames(camera.get(), &ICamera::frameCaptured);
        QSignalSpy errors(camera.get(), &ICamera::captureError);
        QCOMPARE(camera->start(ICamera::Network{QStringLiteral("127.0.0.1"), 1, sensor.localPort()}),
                 ICamera::OperationResult::Pending);
        QTRY_COMPARE(frames.size(), 1);
        QCOMPARE(camera->stopCamera(), ICamera::OperationResult::Pending);
        QCOMPARE(errors.size(), 0);
        const cv::Mat image = qvariant_cast<cv::Mat>(frames.first().first());
        QCOMPARE(image.rows, 4);
        QCOMPARE(image.cols, 1280);
        QCOMPARE(image.type(), CV_8UC1);
        for (int row = 0; row < image.rows; ++row) {
            const auto *pixels = image.ptr<uchar>(row);
            QVERIFY(std::all_of(pixels, pixels + image.cols, [row](uchar px) { return px == 10 + row; }));
        }
        QVERIFY(frames.first().at(1).toLongLong() > 0);
    }

    void recoversAfterIncompleteFrame()
    {
        QUdpSocket sensor;
        QVERIFY(sensor.bind(QHostAddress::LocalHost, 0));
        QNetworkDatagram timedOutRequest;
        int requests = 0;
        connect(&sensor, &QUdpSocket::readyRead, this, [&] {
            while (sensor.hasPendingDatagrams()) {
                const auto request = sensor.receiveDatagram();
                if (request.data() == ReadRows) {
                    reply(sensor, request, RowSettings);
                    continue;
                }
                QCOMPARE(request.data(), Capture);
                if (++requests == 1) {
                    timedOutRequest = request;
                    reply(sensor, request, rowPacket(1, 2, 99));
                } else {
                    QVERIFY(timedOutRequest.senderPort() != request.senderPort());
                    for (int row = 2; row <= 5; ++row)
                        reply(sensor, timedOutRequest, rowPacket(1, row, 99));
                    for (int row = 5; row >= 2; --row)
                        reply(sensor, request, rowPacket(2, row, 42));
                }
            }
        });

        std::unique_ptr<ICamera> camera = std::make_unique<CameraLS2>();
        QSignalSpy frames(camera.get(), &ICamera::frameCaptured);
        QSignalSpy errors(camera.get(), &ICamera::captureError);
        QCOMPARE(camera->start(ICamera::Network{QStringLiteral("127.0.0.1"), 4, sensor.localPort(), 170}),
                 ICamera::OperationResult::Pending);
        QTRY_COMPARE(frames.size(), 1);
        QCOMPARE(camera->stopCamera(), ICamera::OperationResult::Pending);
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().first().toString().contains(QStringLiteral("1/4")));
        QCOMPARE(qvariant_cast<cv::Mat>(frames.first().first()).at<uchar>(0, 0), uchar(42));
        QTest::qWait(300);
        QCOMPARE(requests, 2);
        QCOMPARE(frames.size(), 1);
    }
};

QTEST_GUILESS_MAIN(CameraLS2Tests)
#include "camerals2_tests.moc"
