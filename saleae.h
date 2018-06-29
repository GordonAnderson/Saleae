#ifndef SALEAE_H
#define SALEAE_H

#include <QMainWindow>
#include <QComboBox>
#include "qtcpsocket.h"
#include <QThread>

namespace Ui {
class Saleae;
}

class Saleae : public QMainWindow
{
    Q_OBJECT


public:
    explicit Saleae(QWidget *parent = 0);
    ~Saleae();

public slots:
    void GetDevices();
    void ConnectRequest();
    void ReadDevices();
    void ReadSampleRates();
    void DeviceSelected(QString Device);
    void SetSampleRate(QString SampleRate);
    void CommandResponse();
    void Acquire(bool checked);
    void GetFileName(bool checked);
    void DisplayAboutMessage(void);

private:
    Ui::Saleae *ui;
    QTcpSocket client;
    float aRate,dRate;
    bool CheckProcessing;
    int CurrentChannel;
    void Acquire(void);
    void PopulatecomboBox(QComboBox *combo);
    void SetTrigger(void);
    void SetNumberSamples(void);
    void SetPreTrigger(void);
    void Capture(void);
    void Export(void);

    typedef void (Saleae::*FunctionPtr)(void);

    FunctionPtr PostCommandCall;

    void GetSampleRates(void);
};

#endif // SALEAE_H
