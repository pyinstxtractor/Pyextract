#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>

namespace Ui {
class MainWindow;
}




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
    void onExtractionStarted();
    void onExtractionFinished();
    void onErrorOccurred(const QString &errorMessage);

private:
    Ui::MainWindow *ui;

    QThread *workerThread;
};

#endif // MAINWINDOW_H
