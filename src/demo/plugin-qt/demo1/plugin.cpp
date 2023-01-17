
#include "service.h"
#include <QDBusConnection>

extern "C" int DSMRegisterObject(const char *name, void *data)
{
    (void)data;
    QDBusConnection::RegisterOptions opts = QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties;

    // TODO:session. system
    QDBusConnection::connectToBus(QDBusConnection::SessionBus, QString(name)).registerObject("/org/deepin/services/demo1", new Service(), opts);
    return 0;
}
