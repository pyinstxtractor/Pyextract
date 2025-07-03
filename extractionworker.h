#ifndef EXTRACTIONWORKER_H
#define EXTRACTIONWORKER_H

#include <QObject>
#include <QString>
#include <QMutex>

class ExtractionWorker : public QObject
{
    Q_OBJECT
public:
    explicit ExtractionWorker(const QString &archivePath, const QString &outputDir, const QString &selectedFile = "", QObject *parent = nullptr);

public slots:
    void startExtraction();

signals:
    void progress(int percent);
    void finished();
    void errorOccurred(const QString &message);

private:
    QString m_archivePath;
    QString m_outputDir;
    QString m_selectedFile;  // Store the selected file name
};

#endif // EXTRACTIONWORKER_H
