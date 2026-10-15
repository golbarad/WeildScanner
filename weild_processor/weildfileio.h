#pragma once
#include <QFile>
#include <QSaveFile>
#include <opencv2/core.hpp>

// Qt handles Unicode Windows paths and sandbox file access. OpenCV parses in memory.
namespace WeildFileIO {
inline bool read(const QString &path, cv::FileStorage &storage, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { if(error)*error=file.errorString(); return false; }
    const auto bytes=file.readAll();
    if (file.error()!=QFileDevice::NoError) { if(error)*error=file.errorString(); return false; }
    if (bytes.isEmpty()) { if(error)*error="Empty configuration file"; return false; }
    if (!storage.open(bytes.toStdString(),cv::FileStorage::READ|cv::FileStorage::MEMORY)) {
        if(error)*error="Cannot parse configuration"; return false;
    }
    return true;
}
inline bool write(const QString &path, cv::FileStorage &storage, QString *error) {
    const auto bytes=QByteArray::fromStdString(storage.releaseAndGetString());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {
        if(error)*error=file.errorString(); return false;
    }
    return true;
}
inline int format(const QString &path) {
    return cv::FileStorage::WRITE|cv::FileStorage::MEMORY|
        (path.endsWith(".xml",Qt::CaseInsensitive) ? cv::FileStorage::FORMAT_XML : cv::FileStorage::FORMAT_YAML);
}
}
