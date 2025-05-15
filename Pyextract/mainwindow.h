#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#ifdef Q_OS_WIN
#include "QWinTaskbarButton.h"
#endif

#include <QMainWindow>
#include <QThread>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QShowEvent>

namespace Ui {
class MainWindow;
}

class ExtractionWorker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void processFile(const QString &filePath);

private slots:
    void onSelectFileButtonClicked();
    void onExtractButtonClicked();
    void onExtractionFinished();
    void onErrorOccurred(const QString &errorMessage);
    void onExtractionProgress(int progress);
    void onExtractionStarted();

private:
#ifdef _WIN32
    QWinTaskbarButton* taskbarButton;
    QWinTaskbarProgress* taskbarProgress;
#endif
    Ui::MainWindow *ui;
    QThread *workerThread;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    #ifdef _WIN32
    void showEvent(QShowEvent *event) override;
    #endif
};

#endif // MAINWINDOW_H
