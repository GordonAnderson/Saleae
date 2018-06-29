//
// Saleae
//
//  This application uses the Socket API interface to "talk" to the Saleae Logic application.
//  Make sure the Logic app is started before you try to connect using this application.
//  This application is desiged for FTICR data acqusistion specufically for Jim Bruce's
//  array project. The Logic device is setup for triggering on its last channel and the user
//  can select the number of channels to record and various record parameters.
//
//  The status bar (at the bottom of the dialog box, show the systems status. If the Logic app
//  responds with a NAK then the processing will halt.
//
// Written by: Gordon Anderson
//
// Revision history:
//  Rev 1.0 June 26, 2015
//      1.) Initial version
//
// To do list:
//
#include "saleae.h"
#include "ui_saleae.h"
#include "qtcpsocket.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QIntValidator>

// Constructor
Saleae::Saleae(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Saleae)
{
    ui->setupUi(this);
    // Make the dialog fixed size.
    this->setFixedSize(this->size());

    ui->leNumberSamples->setValidator(new QIntValidator);
    ui->lePreTrigger->setValidator(new QIntValidator);
    // Init the variables
    PostCommandCall = NULL;
    CheckProcessing = false;
    // Connect the processing actions
    connect(ui->pbConnect, SIGNAL(clicked(bool)), this, SLOT(ConnectRequest()));
    connect(&client, SIGNAL(connected()), this, SLOT(GetDevices()));
    connect(&client, SIGNAL(readyRead()), this, SLOT(ReadDevices()));
    connect(ui->comboDevices, SIGNAL(currentIndexChanged(QString)), this, SLOT(DeviceSelected(QString)));
    connect(ui->comboSampleRates, SIGNAL(currentIndexChanged(QString)), this, SLOT(SetSampleRate(QString)));
    connect(ui->pbAcquire,SIGNAL(clicked(bool)), this, SLOT(Acquire(bool)));
    connect(ui->pbFileSelect,SIGNAL(clicked(bool)),this,SLOT(GetFileName(bool)));
    connect(ui->actionAbout,SIGNAL(triggered(bool)), this, SLOT(DisplayAboutMessage()));
}

Saleae::~Saleae()
{
    delete ui;
}

void Saleae::DisplayAboutMessage(void)
{
    QMessageBox::information(
        this,
        tr("Application Named"),
        tr("Saleae Logic interface application, written by Gordon Anderson. This application is used for FTICR data collection.") );
}

void Saleae::GetFileName(bool checked)
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),"",tr("Files (*.*)"));
    ui->leFileName->setText(fileName);
}


// The following functions are called in the order listed to acquire data from the Logic
// device:
//      Acquire (fired from the Acquire push button, this is a slot function and overloaded)
//      SetTrigger
//      SetNumberSamples
//      SetPreTrigger
//      Capture
//      Export  (Called multiple times, one time for each channel recorded. If repeat is checked
//               then the Acquire function is called after final dat export)
// Each call results in the CommandResponse slot function being called and if a ACK is received then
// the next function is called using the function pointer provided. This way the system "walks"
// through the aquire steps.
// If a NAK is received the processing cascade stops, the status bar will show where the cascade
// stopped.
void Saleae::Acquire(void)
{
    Acquire(false);
}

// Called when Acquire push button is pressed. Set up the active channels and fire the
// set trigger function on ACK.
void Saleae::Acquire(bool checked)
{
    QString str;
    int i;

    if(!client.isOpen()) return;
    // Set the active channels
    str = "set_active_channels, digital_channels,";
    str += ui->leTriggerChan->text();
    str += ",analog_channels";
    for(i=0; i< ui->comboNumChannels->currentText().toInt(); i++)
    {
       str += ",";
       str += QString::number(i);
    }
    statusBar()->showMessage(str);
    PostCommandCall = &Saleae::SetTrigger;
    disconnect(&client,SIGNAL(readyRead()),0,0);
    connect(&client,SIGNAL(readyRead()),this,SLOT(CommandResponse()));
    // Set the active channels
    statusBar()->showMessage(tr("Set active channels: "));
    client.write(str.toStdString().c_str());
    client.write("",1);
}

// Set the trigger options and fire the set number of samples function
// on ACK.
void Saleae::SetTrigger(void)
{
    QString str;

    str = "set_trigger,";
    if(ui->rbHigh->isChecked()) str += "high";
    if(ui->rbLow->isChecked()) str += "low";
    if(ui->rbNegedge->isChecked()) str += "negedge";
    if(ui->rbPosedge->isChecked()) str += "posedge";
    // Set trigger
    PostCommandCall = &Saleae::SetNumberSamples;
    statusBar()->showMessage(tr("Set trigger: "));
    client.write(str.toStdString().c_str());
    client.write("",1);
}

// Sets the number of samples and fires set pre trigger
void Saleae::SetNumberSamples(void)
{
    QString str;

    str = "set_num_samples," + QString::number(ui->leNumberSamples->text().toInt() * dRate / aRate,'f',0);
    PostCommandCall = &Saleae::SetPreTrigger;
    statusBar()->showMessage(tr("Set number of samples: "));
    client.write(str.toStdString().c_str());
    client.write("",1);
}

// Sets the pre trigger and files capture
void Saleae::SetPreTrigger(void)
{
    QString str;

    str = "set_capture_pretrigger_buffer_size," + ui->lePreTrigger->text();
    PostCommandCall = &Saleae::Capture;
    statusBar()->showMessage(tr("Set pre-trigger: "));
    client.write(str.toStdString().c_str());
    client.write("",1);
}

// Sends the capture command and waits for an ACK as well and a finishied
// processing flag before advancing to export the data.
void Saleae::Capture(void)
{
    QString str;

    str = "capture";
    PostCommandCall = &Saleae::Export;
    CheckProcessing = true;
    CurrentChannel = 0;
    statusBar()->showMessage(tr("Capture: "));
    client.write(str.toStdString().c_str());
    client.write("",1);
}

// This function exports one channel of the data to a file on each call. This function fires itself
// until all the channels are read and then stops of called Acquire again is the repeat check box
// is selected.
void Saleae::Export(void)
{
    int  i,extNum;
    bool Ok;
    QString   str,fileName,message;
    QFileInfo obFileInfo(ui->leFileName->text());

    fileName = obFileInfo.absolutePath() + "/" + obFileInfo.baseName() + "_" + QString::number(CurrentChannel) + "." + obFileInfo.completeSuffix();
    str = "export_data," + fileName + ",analog_channels," + QString::number(CurrentChannel);
 //   str += ", voltage, time_span, 0, " + QString::number(ui->leNumberSamples->text().toFloat() / aRate) + ", binary, each_sample, 32";
    str += ", voltage, all_time, binary, each_sample, 32";
//    qDebug() << str;
    message = "Exporting channel " + QString::number(CurrentChannel) + ": ";
    statusBar()->showMessage(message);
    CurrentChannel++;
    if(CurrentChannel < ui->comboNumChannels->currentText().toInt()) PostCommandCall = &Saleae::Export;
    else
    {
        if(ui->chkRepeat->isChecked() == true) PostCommandCall = &Saleae::Acquire;
        else PostCommandCall = NULL;
        // Advance filename if the extension is a number
        extNum = obFileInfo.completeSuffix().toInt(&Ok);
        if(Ok)
        {
            extNum++;
            QString newExt = QString("%1").arg(extNum, obFileInfo.completeSuffix().length(), 10, QLatin1Char('0'));
            ui->leFileName->setText(obFileInfo.absolutePath() + "/" + obFileInfo.baseName() + "." + newExt);
        }
    }
    CheckProcessing = true;
    client.write(str.toStdString().c_str());
    client.write("",1);
}
// End for acquire routines.

void Saleae::SetSampleRate(QString SampleRate)
{
    QStringList SampleRates;

    if(SampleRate == "") return;
    SampleRates = SampleRate.split(",");
    aRate = SampleRates[1].toFloat();
    dRate = SampleRates[0].toFloat();
    PostCommandCall = NULL;
    disconnect(&client,SIGNAL(readyRead()),0,0);
    connect(&client,SIGNAL(readyRead()),this,SLOT(CommandResponse()));
    // Select the sample rate
    statusBar()->showMessage(tr("Select sample rate: "));
    client.write("set_sample_rate,");
    client.write(SampleRate.toStdString().c_str());
    client.write("",1);
}

void Saleae::GetSampleRates(void)
{
   // If here we need to read all the sample rates and update the combo box
   disconnect(&client,SIGNAL(readyRead()),0,0);
   connect(&client,SIGNAL(readyRead()),this,SLOT(ReadSampleRates()));
   client.write("get_all_sample_rates");
   client.write("",1);
}

void Saleae::DeviceSelected(QString Device)
{
   QStringList DeviceArgs;
   int i,j;

   if(Device == "") return;
   DeviceArgs = Device.split(",");
   i = DeviceArgs[1].right(2).toInt();
   ui->leTriggerChan->setText(QString::number(i-1));
   ui->comboNumChannels->clear();
   for(j=0;j<(i-1);j++)
   {
       ui->comboNumChannels->addItem(QString::number(j+1));
   }
   disconnect(&client,SIGNAL(readyRead()),0,0);
   connect(&client,SIGNAL(readyRead()),this,SLOT(CommandResponse()));
   // Select the device
   PostCommandCall = &Saleae::GetSampleRates;
   statusBar()->showMessage(tr("Select active device: "));
   client.write("select_active_device,");
   client.write(DeviceArgs[0].toStdString().c_str());
   client.write("",1);
}

void Saleae::ConnectRequest()
{
    if(client.isOpen()) client.close();
    client.connectToHost("127.0.0.1", 10429);
}

void Saleae::GetDevices()
{
    client.write("get_connected_devices",22);
    statusBar()->showMessage(tr("Connected!"));
}

// This is the command response slot function. This is used when  ACK or NAK
// response from the Logic app is expected. This function can call a post ACK
// function if the PostCommandCall is not NULL. This function cal also wait for
// all processing in the Logic app to complete is the CheckProcessing flag is
// True.
void Saleae::CommandResponse()
{
    QString str;
    static bool WaitForProcessing = false;

    str = client.readAll();
    if(WaitForProcessing)
    {
        if(!str.contains("TRUE"))
        {
            QThread::msleep(10);
            client.write("is_processing_complete");
            client.write("",1);
            return;
        }
        WaitForProcessing = false;
        CheckProcessing = false;
    }
    else
    {
        statusBar()->showMessage(statusBar()->currentMessage() + str);
    }
    if(str == "NAK") return;
    // Here with ACK, if CheckProcessing flag is sent then set this function's Wait for
    // processing call and issue the check command else we are done
    if(CheckProcessing)
    {
        WaitForProcessing=true;
        client.write("is_processing_complete");
        client.write("",1);
        return;
    }
    if(PostCommandCall != NULL)  (this->*PostCommandCall)();
}

void Saleae::ReadDevices()
{
    PopulatecomboBox(ui->comboDevices);
}

void Saleae::ReadSampleRates()
{
    PopulatecomboBox(ui->comboSampleRates);
}

void Saleae::PopulatecomboBox(QComboBox *combo)
{
    QString str;
    QStringList DeviceList;
    int i;

    combo->clear();
    combo->addItem("");
    str = client.readAll();
    DeviceList = str.split(char(10));
    for(i=0;i<DeviceList.count();i++)
    {
        if(DeviceList[i] != "ACK")
           combo->addItem(DeviceList[i]);
    }
}
