#include "QtSerialCom.h"
#include <QSerialPortInfo>
#include <sql.h>

QtSerialCom::QtSerialCom(QWidget* parent)
    : QMainWindow(parent), port(nullptr)
{
    ui.setupUi(this);

    QList<QSerialPortInfo> availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : availablePorts)
    {
        ui.comboBox->addItem(info.portName(), QVariant(info.portName()));
    }
}

QtSerialCom::~QtSerialCom()
{
    if (port && port->isOpen())
        port->close();
    delete port;
}

void QtSerialCom::OpenPort()
{
    if (ui.comboBox->currentIndex() >= 0)
    {
        if (port)
        {
            if (port->isOpen())
                port->close();
            delete port;
        }

        port = new QSerialPort(ui.comboBox->currentText(), this);
        QObject::connect(port, &QSerialPort::readyRead, this, &QtSerialCom::onSerialPortReadyRead);

        port->setBaudRate(QSerialPort::Baud9600);
        port->setDataBits(QSerialPort::Data8);
        port->setParity(QSerialPort::NoParity);
        port->setStopBits(QSerialPort::OneStop);
        port->setFlowControl(QSerialPort::NoFlowControl);

        if (port->open(QIODevice::ReadWrite))
        {
            ui.PortStatus->setText("Status port : Ouvert");
        }
        else
        {
            ui.PortStatus->setText("Status port : Échec d'ouverture");
            delete port;
            port = nullptr;
        }
    }
}


void QtSerialCom::onSerialPortReadyRead()
{
    static QByteArray buffer;
    buffer.append(port->read(port->bytesAvailable()));

    if (buffer.contains("$GPGGA") && buffer.contains("\r\n"))
    {
        int startIdx = buffer.indexOf("$GPGGA");
        int endIdx = buffer.indexOf("\r\n", startIdx);
        QByteArray frame = buffer.mid(startIdx, endIdx - startIdx + 2);
        buffer.remove(0, endIdx + 2);

        QStringList fields = QString(frame).split(',');
        if (fields.size() > 6)
        {
            // Conversion des coordonnées
            double lat = fields[2].left(2).toDouble() + fields[2].mid(2).toDouble() / 60.0;
            if (fields[3] == "S") lat = -lat;
            double lon = fields[4].left(3).toDouble() + fields[4].mid(3).toDouble() / 60.0;
            if (fields[5] == "W") lon = -lon;

            // Connexion à la base MySQL
            MYSQL* conn = mysql_init(nullptr);
            if (conn == nullptr)
            {
                qDebug() << "Erreur MySQL init : " << mysql_error(conn);
                return;
            }

            if (mysql_real_connect(conn, "192.168.64.132", "admintest", "root", "Lowrance", 0, nullptr, 0) == nullptr)
            {
                qDebug() << "Erreur de connexion MySQL : " << mysql_error(conn);
                mysql_close(conn);
                return;
            }

            // Création et exécution de la requête
            QString query = QString("INSERT INTO gpsdata (longitude, latitude) VALUES (%1, %2)").arg(lon).arg(lat);
            if (mysql_query(conn, query.toStdString().c_str()))
            {
                qDebug() << "Erreur lors de l'insertion : " << mysql_error(conn);
            }
            else
            {
                qDebug() << "Insertion réussie : Longitude = " << lon << ", Latitude = " << lat;
            }

            // Fermeture de la connexion
            mysql_close(conn);
        }
    }
}