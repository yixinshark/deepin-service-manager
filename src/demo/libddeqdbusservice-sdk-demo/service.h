#ifndef SERVICE_H
#define SERVICE_H

#include "ddeqdbusservice.h"

#include <QObject>
#include <QDBusContext>

class Service : public DDEQDBusService,
                protected QDBusContext
{
    Q_OBJECT
public:
    explicit Service(QObject *parent = 0);

    Q_PROPERTY(QString msg READ Msg WRITE SetMsg NOTIFY MsgChanged)
    QString Msg();
    void SetMsg(QString value);

signals:
    // xml - signal
    void MsgChanged(const QString &msg);

    void dbusLog(const QString &text);

public slots:
    // xml - method
    QString Hello();
    bool Register(const QString &id, QString &result2);

private:
    QString m_msg;
};

#endif // SERVICE_H
