#include "arduino.h"
#include <QSerialPortInfo>

QStringList Arduino::ports_disponibles()
{
    QStringList ports;
    for (const auto &info : QSerialPortInfo::availablePorts())
        ports.append(info.portName());
    return ports;
}

bool Arduino::connect_arduino(const QString &portName)
{
    close_arduino();
    serial.setPortName(portName);
    return serial.setBaudRate(QSerialPort::Baud9600)
        && serial.setDataBits(QSerialPort::Data8)
        && serial.setParity(QSerialPort::NoParity)
        && serial.setStopBits(QSerialPort::OneStop)
        && serial.setFlowControl(QSerialPort::NoFlowControl)
        && serial.open(QIODevice::ReadWrite);
}

void Arduino::close_arduino()
{
    if (serial.isOpen())
        serial.close();
    ligne.clear();
    ignorerLigne = false;
}

bool Arduino::write_to_arduino(const QByteArray &message)
{
    const QByteArray trame = message + '\n';
    return serial.isOpen() && serial.write(trame) == trame.size();
}

QList<QByteArray> Arduino::read_from_arduino()
{
    QList<QByteArray> messages;
    const QByteArray donnees = serial.readAll();
    for (char octet : donnees) {
        if (octet == '\n') {
            if (!ignorerLigne)
                messages.append(ligne.trimmed());
            ligne.clear();
            ignorerLigne = false;
        } else if (!ignorerLigne) {
            if (ligne.size() < 64)
                ligne.append(octet);
            else
                ignorerLigne = true;
        }
    }
    return messages;
}
