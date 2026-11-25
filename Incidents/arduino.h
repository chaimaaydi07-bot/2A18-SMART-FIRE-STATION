#ifndef ARDUINO_H
#define ARDUINO_H

#include <QSerialPort>
#include <QList>
#include <QStringList>

class Arduino
{
public:
    static QStringList ports_disponibles();
    bool connect_arduino(const QString &portName);
    void close_arduino();
    bool write_to_arduino(const QByteArray &message);
    QList<QByteArray> read_from_arduino();
    QSerialPort *getserial() { return &serial; }

private:
    QSerialPort serial;
    QByteArray ligne;
    bool ignorerLigne = false;
};

#endif
