#include "widget.h"
#include "ui_widget.h"
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>
#include <QtWin>
#include <QPropertyAnimation>
#include <QPainter>
#include <QPaintEvent>
#include "AudioDevice.h"
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_NoSystemBackground); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    setAttribute(Qt::WA_TranslucentBackground); //加上这句使得客户区也生效Blur
    QtWin::taskbarDeleteTab(this); //删除任务栏图标

    QPropertyAnimation *anima_alpha = new QPropertyAnimation(this, "windowOpacity");
    anima_alpha->setDuration(750);
    anima_alpha->setStartValue(0);
    anima_alpha->setEndValue(1.0);
    connect(anima_alpha, &QPropertyAnimation::finished, this, [=](){
        if(anima_alpha->direction() == QAbstractAnimation::Backward)
            this->hide();
    });

    static auto display = [=](const QString& name, int volume){
        ui->label_name->setText(name);
        ui->label_vol->setText(QString::number(volume));
        this->update(); //否则label有黑边
        anima_alpha->setDirection(QAbstractAnimation::Forward);
        anima_alpha->start();
        show();

//        ui->label->setPixmap(QString(":/airpods.png"));

        QTimer::singleShot(4000, this, [=](){
            anima_alpha->setDirection(QAbstractAnimation::Backward);
            anima_alpha->start();
        });
    };


    QBluetoothDeviceDiscoveryAgent* btFinder = new QBluetoothDeviceDiscoveryAgent(this);
    connect(btFinder, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, [=](const QBluetoothDeviceInfo &device){
        if(!device.serviceClasses().testFlag(QBluetoothDeviceInfo::AudioService)) return; //isAudio BT

        QString btName = device.name();
        QString audioName = AudioDevice::defaultOutputDevice().getPureName();
        if(btName == audioName){ //连接的是蓝牙音频设备
            btFinder->stop();
            int volume = AudioDevice::getVolume();
            int lastVolume = audioVolume.value(btName, volume);
            qDebug() << btName << volume << "to" << lastVolume;
            display(btName, lastVolume);
            if(volume != lastVolume)
                AudioDevice::setVolume(lastVolume); //恢复上次音量
        }
    });
    connect(btFinder, &QBluetoothDeviceDiscoveryAgent::canceled, this, [=](){
        qDebug() << "#Bluetooth finder canceled";
    });
    connect(btFinder, &QBluetoothDeviceDiscoveryAgent::finished, this, [=](){
        qDebug() << "#Bluetooth finder finished";
    });

    QTimer* timer = new QTimer(this);
    timer->callOnTimeout([=](){
        QString dev = AudioDevice::defaultOutputDevice().getPureName();
        int volume = AudioDevice::getVolume();
        if(lastDev != dev){
            qDebug() << "#New AudioDevice Founded" << dev;
            qDebug() << "#Bluetooth finder started";
            btFinder->stop();
            btFinder->start(); //确定是否是蓝牙耳机
            lastDev = dev;
        }else{ //出现过：音频切换 音量切换 但是lastDev == dev的情况
            if(volume != audioVolume[dev]){
                audioVolume[dev] = AudioDevice::getVolume(); //记录所有音频设备音量，此时无需判断是否是蓝牙，性价比太低
                qDebug() << "Record:" << dev << audioVolume[dev];
            }
        }
    });
    timer->start(1000);

}

Widget::~Widget()
{
    delete ui;
}

void Widget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(event->rect(), 20, 20);
}
