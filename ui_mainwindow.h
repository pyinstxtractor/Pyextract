/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.7.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QLabel *label_fileName;
    QLineEdit *textbox;
    QPushButton *Openbutton;
    QPushButton *Extractbutton;
    QListWidget *listWidget;
    QProgressBar *progressbar;
    QMenuBar *menubar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(435, 477);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        label_fileName = new QLabel(centralwidget);
        label_fileName->setObjectName("label_fileName");

        verticalLayout->addWidget(label_fileName);

        textbox = new QLineEdit(centralwidget);
        textbox->setObjectName("textbox");
        textbox->setDragEnabled(false);
        textbox->setReadOnly(true);

        verticalLayout->addWidget(textbox);

        Openbutton = new QPushButton(centralwidget);
        Openbutton->setObjectName("Openbutton");

        verticalLayout->addWidget(Openbutton);

        Extractbutton = new QPushButton(centralwidget);
        Extractbutton->setObjectName("Extractbutton");

        verticalLayout->addWidget(Extractbutton);

        listWidget = new QListWidget(centralwidget);
        listWidget->setObjectName("listWidget");

        verticalLayout->addWidget(listWidget);

        progressbar = new QProgressBar(centralwidget);
        progressbar->setObjectName("progressbar");

        verticalLayout->addWidget(progressbar);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 435, 21));
        MainWindow->setMenuBar(menubar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        label_fileName->setText(QCoreApplication::translate("MainWindow", "File Name:", nullptr));
        Openbutton->setText(QCoreApplication::translate("MainWindow", "Browse", nullptr));
        Extractbutton->setText(QCoreApplication::translate("MainWindow", "Extract", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
