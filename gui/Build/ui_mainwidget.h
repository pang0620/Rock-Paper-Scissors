/********************************************************************************
** Form generated from reading UI file 'mainwidget.ui'
**
** Created by: Qt User Interface Compiler version 5.15.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWIDGET_H
#define UI_MAINWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_mainwidget
{
public:
    QWidget *verticalLayoutWidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QGraphicsView *graphicsView_2;
    QGraphicsView *graphicsView;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *pushButton_2;
    QPushButton *pushButton;

    void setupUi(QWidget *mainwidget)
    {
        if (mainwidget->objectName().isEmpty())
            mainwidget->setObjectName(QString::fromUtf8("mainwidget"));
        mainwidget->resize(800, 600);
        verticalLayoutWidget = new QWidget(mainwidget);
        verticalLayoutWidget->setObjectName(QString::fromUtf8("verticalLayoutWidget"));
        verticalLayoutWidget->setGeometry(QRect(10, 10, 781, 571));
        verticalLayout = new QVBoxLayout(verticalLayoutWidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        graphicsView_2 = new QGraphicsView(verticalLayoutWidget);
        graphicsView_2->setObjectName(QString::fromUtf8("graphicsView_2"));

        horizontalLayout->addWidget(graphicsView_2);

        graphicsView = new QGraphicsView(verticalLayoutWidget);
        graphicsView->setObjectName(QString::fromUtf8("graphicsView"));

        horizontalLayout->addWidget(graphicsView);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        pushButton_2 = new QPushButton(verticalLayoutWidget);
        pushButton_2->setObjectName(QString::fromUtf8("pushButton_2"));

        horizontalLayout_2->addWidget(pushButton_2);

        pushButton = new QPushButton(verticalLayoutWidget);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));

        horizontalLayout_2->addWidget(pushButton);


        verticalLayout->addLayout(horizontalLayout_2);


        retranslateUi(mainwidget);

        QMetaObject::connectSlotsByName(mainwidget);
    } // setupUi

    void retranslateUi(QWidget *mainwidget)
    {
        mainwidget->setWindowTitle(QCoreApplication::translate("mainwidget", "mainwidget", nullptr));
        pushButton_2->setText(QCoreApplication::translate("mainwidget", "Camera Open", nullptr));
        pushButton->setText(QCoreApplication::translate("mainwidget", "Start", nullptr));
    } // retranslateUi

};

namespace Ui {
    class mainwidget: public Ui_mainwidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWIDGET_H
