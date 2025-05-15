#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QProgressBar>
#include <QMutex>
#include <QMimeData>
#include <QDropEvent>
#include <QDebug>


#ifdef Q_OS_WIN
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#endif
#include <qtimer.h>

#include "mainwindow.h"
#include "extractionworker.h"
#include "ui_mainwindow.h"
#include "pyinstarchive.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("PyInstaller Archive Viewer");
#ifdef Q_OS_WIN
    taskbarButton = new QWinTaskbarButton(this);
    taskbarButton->setWindow(windowHandle());

    taskbarProgress = taskbarButton->progress();
    taskbarProgress->setRange(0, 100);
    taskbarProgress->setVisible(true);
#endif
    ui->progressbar->setStyleSheet("QProgressBar {"
                                   "   height: 30px;"
                                   "   width: 300px;"
                                   "   border: 2px solid gray;"
                                   "   border-radius: 5px;"
                                   "   text-align: center;"
                                   "}"
                                   "QProgressBar::chunk {"
                                   "   background-color: #0078D4;"
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

#ifdef Q_OS_WIN
void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // Initialize taskbar button and progress
    taskbarButton = new QWinTaskbarButton(this);
    taskbarButton->setWindow(this->windowHandle());

    taskbarProgress = taskbarButton->progress();
    taskbarProgress->setMinimum(0);
    taskbarProgress->setMaximum(100);
}
#endif

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
    QString fileFilter;

#ifdef Q_OS_WIN
    fileFilter = "All Files (*.*)";
#elif defined(Q_OS_MAC)
    fileFilter = "All Files (*)";  // macOS: no extension, allow all
#else
    fileFilter = "All Files (*)";  // fallback
#endif

    QString filePath = QFileDialog::getOpenFileName(this, "Open Archive File", "", fileFilter);
    if (!filePath.isEmpty()) {
        ui->textbox->setText(filePath);
        processFile(filePath);
    }
}

void MainWindow::processFile(const QString &filePath)
{
    std::string stdFilePath = filePath.toStdString();
    PyInstArchive archive(stdFilePath);


#ifdef Q_OS_WIN
    // Initialize taskbar progress only
    if (taskbarProgress) {
        taskbarProgress->setValue(0);
        taskbarProgress->setVisible(true);
    }
#endif
    // Step 1: Open the Archive
    if (!archive.open()) {
        qDebug() << "[-] File open failed.";
        QMessageBox::warning(this, "Error", "Failed to open the file.");
        return;
    }
    qDebug() << "[+] File open!";
	#ifdef Q_OS_WIN
    taskbarProgress->setValue(25);
#endif
    // Step 2: Check File Validity
    if (!archive.checkFile()) {
        qDebug() << "[-] File check failed.";
        QMessageBox::warning(this, "Error", "Invalid file format.");
        return;
    }
    qDebug() << "[+] File check passed!";
#ifdef Q_OS_WIN
    taskbarProgress->setValue(50);
#endif
    // Step 3: Extract Archive Information
    if (!archive.getCArchiveInfo()) {
        qDebug() << "[-] Failed to extract archive info.";
        QMessageBox::warning(this, "Error", "Failed to extract archive information.");
        return;
    }
    qDebug() << "[+] Archive info extracted successfully!";
#ifdef Q_OS_WIN
    taskbarProgress->setValue(100);

    // Hide taskbar progress once complete
    QTimer::singleShot(1000, this, [this]() {
        taskbarProgress->hide();
    });
#endif
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

    QString selectedFile;
    QListWidgetItem *item = ui->listWidget->currentItem();
    if (item) {
        selectedFile = item->text();
    }

    workerThread = new QThread;
    auto *worker = new ExtractionWorker(archivePath, outputDir, selectedFile);
    worker->moveToThread(workerThread);

    // Ensure the progress bar is visible
    ui->progressbar->setValue(0);
    ui->progressbar->show();
#ifdef Q_OS_WIN
    if (taskbarProgress) {
        taskbarProgress->setValue(0);
        taskbarProgress->setVisible(true);
    }
#endif

    connect(workerThread, &QThread::started, worker, &ExtractionWorker::startExtraction);

    connect(worker, &ExtractionWorker::progress, this, [=](int value) {
        ui->progressbar->setValue(value);
#ifdef Q_OS_WIN
        if (taskbarProgress) {
            taskbarProgress->setValue(value);
        }
#endif
    });

    connect(worker, &ExtractionWorker::finished, this, &MainWindow::onExtractionFinished);
    connect(worker, &ExtractionWorker::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(worker, &ExtractionWorker::finished, workerThread, &QThread::quit);

    workerThread->start();
    onExtractionStarted();
}

void MainWindow::onExtractionStarted()
{
    ui->progressbar->setValue(0);
}

void MainWindow::onExtractionProgress(int progress)
{
    ui->progressbar->setValue(progress);
#ifdef Q_OS_WIN
    if (taskbarProgress) {
        taskbarProgress->setValue(progress);
    }
#endif
}

void MainWindow::onExtractionFinished()
{
    QMessageBox::information(this, "Success", "Extraction complete!");
    ui->progressbar->setValue(100);
#ifdef Q_OS_WIN
    if (taskbarProgress) {
        taskbarProgress->setValue(100);
        taskbarProgress->hide(); //  hide once done
    }
#endif
}

void MainWindow::onErrorOccurred(const QString& errorMessage)
{
    QMessageBox::critical(this, "Error", errorMessage);
}
