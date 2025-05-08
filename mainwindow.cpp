#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "pyinstarchive.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QProgressBar>
#include <QMutex>
#include <QMimeData>
#include <QDropEvent>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("PyInstaller Archive Viewer");
    ui->progressbar->setStyleSheet("QProgressBar {"
                                   "   height: 30px;"
                                   "   width: 300px;"
                                   "   border: 2px solid gray;"
                                   "   border-radius: 5px;"
                                   "   text-align: center;"
                                   "}"
                                   "QProgressBar::chunk {"
                                   "   background-color: #4caf50;"
                                   "}");
    ui->textbox->setAcceptDrops(true);
    ui->textbox->setDragEnabled(true);
    connect(ui->Openbutton, &QPushButton::clicked, this, &MainWindow::onSelectFileButtonClicked);
    connect(ui->Extractbutton, &QPushButton::clicked, this, &MainWindow::onExtractButtonClicked);
}

MainWindow::~MainWindow()
{
    workerThread->quit();
    workerThread->wait();
    delete ui;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urlList = event->mimeData()->urls();
        if (!urlList.isEmpty()) {
            QString filePath = urlList.first().toLocalFile();
            ui->textbox->setText(filePath);
            processFile(filePath);

        }
    }
}

void MainWindow::onSelectFileButtonClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open Archive File", "", "PyInstaller Archives (*.exe)");
    if (!filePath.isEmpty()) {
        ui->textbox->setText(filePath);
        processFile(filePath);
    }
}

void MainWindow::processFile(const QString &filePath)
{
    std::string stdFilePath = filePath.toStdString();
    PyInstArchive archive(stdFilePath);

    if (!archive.open()) {
        qDebug() << "[-] File open failed.";
        QMessageBox::warning(this, "Error", "Failed to open the file.");
        return;
    }

    qDebug() << "[+] File open!";

    if (!archive.checkFile()) {
        qDebug() << "[-] File check failed.";
        QMessageBox::warning(this, "Error", "Invalid file format.");
        return;
    }

    qDebug() << "[+] File check passed!";

    if (!archive.getCArchiveInfo()) {
        qDebug() << "[-] Failed to extract archive info.";
        QMessageBox::warning(this, "Error", "Failed to extract archive information.");
        return;
    }

    qDebug() << "[+] Archive info extracted successfully!";

    archive.displayInfo(ui->listWidget);
}

void MainWindow::onExtractButtonClicked()
{
    QString archivePath = ui->textbox->text();
    if (archivePath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a file first.");
        return;
    }

    QString outputDir = QFileDialog::getExistingDirectory(this, "Select Output Directory", "");
    if (outputDir.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select an output directory.");
        return;
    }

}

void MainWindow::onExtractionStarted()
{
    ui->progressbar->setValue(0);
}

void MainWindow::onExtractionFinished()
{
}

void MainWindow::onErrorOccurred(const QString &errorMessage)
{
    QMessageBox::critical(this, "Error", errorMessage);
}
