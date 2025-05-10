#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "QWinTaskbarButton.h"
#include <QMainWindow>
#include <QThread>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>

namespace Ui {
class MainWindow;
}

// Forward declaration for the ExtractionWorker class
class ExtractionWorker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void processFile(const QString &filePath);

private slots:
    void onSelectFileButtonClicked();
    void onExtractButtonClicked();
    void onExtractionFinished();
    void onErrorOccurred(const QString &errorMessage);
    void onExtractionProgress(int progress);
    void onExtractionStarted();

private:
    QWinTaskbarButton *taskbarButton;
    QWinTaskbarProgress *taskbarProgress;
    Ui::MainWindow *ui;
    QThread *workerThread; // Thread for the extraction worker
protected:
    void showEvent(QShowEvent *event) override;
};

#endif // MAINWINDOW_H
