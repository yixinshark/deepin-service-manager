#include "policy/policy.h"

#include <QFile>
#include <QDebug>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

Policy::Policy(QObject *parent) : QObject(parent)
    , m_group("core")
{

}

bool Policy::CheckPathHide(QString path)
{
    QMapPathHide::iterator iter = m_mapPathHide.find(path);
    if (iter == m_mapPathHide.end()) {
        return false;
    }
    return iter.value();
}

bool Policy::CheckMethodPermission(QString process, QString path, QString interface, QString method)
{
    return CheckPermission(process, path, interface, method, CallDestType::Method);
}

bool Policy::CheckPropertyPermission(QString process, QString path, QString interface, QString property)
{
    return CheckPermission(process, path, interface, property, CallDestType::Property);
}

bool Policy::CheckPermission(QString process, QString path, QString interface, QString dest, CallDestType type)
{
    qInfo() << "CheckPermission:" << QString("process=%1, path=%2, interface=%3, dest=%4").arg(process, path, interface, dest);
    // 时间复杂度跟权限配置复杂度正相关，简单的配置就会校验很快
    QMapPath::const_iterator iterPath = m_mapPath.find(path);
    if (iterPath == m_mapPath.end()) {
        // 默认不校验，即有权限
        return true;
    }

    // PATH权限
    const PolicyPath &policyPath = iterPath.value();
    QMapInterface::const_iterator iterInterface = policyPath.interfaces.find(interface);
    if (iterInterface == policyPath.interfaces.end()) {
        // 没有配置interface权限，则用path权限
        if (!policyPath.needPermission) {
            return true;
        }
        return policyPath.processes.contains(process);
    }

    if (type == CallDestType::Method) {
        // INTERFACE权限
        const PolicyInterface &policyInterface = iterInterface.value();
        QMapMethod::const_iterator iterMethod = policyInterface.methods.find(dest);
        if (iterMethod == policyInterface.methods.end()) {
            if (!policyInterface.needPermission) {
                return true;
            }
            return policyInterface.processes.contains(process);
        }
        // METHOD权限
        const PolicyMethod &policyMethod = iterMethod.value();
        if (!policyMethod.needPermission) {
            return true;
        }
        return policyMethod.processes.contains(process);

    } else if (type == CallDestType::Property) {
        // INTERFACE权限
        const PolicyInterface &policyInterface = iterInterface.value();
        QMapProperty::const_iterator iterProp = policyInterface.properties.find(dest);
        if (iterProp == policyInterface.properties.end()) {
            if (!policyInterface.needPermission) {
                return true;
            }
            return policyInterface.processes.contains(process);
        }
        // PROPERTY权限
        const PolicyProperty &policyProp = iterProp.value();
        if (!policyProp.needPermission) {
            return true;
        }
        return policyProp.processes.contains(process);
    }

    qInfo() << "check permission error";
    return false;
}

void Policy::Print()
{
    qInfo() << "-------------------------------------";
    qInfo() << "DBUS POLICY CONFIG";
    qInfo() << "- name:" << m_name;
    qInfo() << "- path hide";
    for (QMapPathHide::iterator iter = m_mapPathHide.begin(); iter != m_mapPathHide.end(); iter++) {
        qInfo() << "-- path hide:" << iter.key() << iter.value();
    }
    qInfo() << "- whitelist";
    for (QMapWhitelists::iterator iter = m_mapWhitelist.begin(); iter != m_mapWhitelist.end(); iter++) {
        qInfo() << "-- whitelist:" << iter.key() << iter.value().name << iter.value().process;
    }
    qInfo() << "- policy";
    for (QMapPath::iterator iter = m_mapPath.begin(); iter != m_mapPath.end(); iter++) {
        qInfo() << "-- path:" << iter.key() << iter.value().path;
        qInfo() << "-- permission:" << iter.value().needPermission;
        qInfo() << "-- whitelist:" << iter.value().processes;
        for (QMapInterface::iterator iterInterface = iter.value().interfaces.begin();
             iterInterface != iter.value().interfaces.end(); iterInterface++) {
            qInfo() << "---- interface:" << iterInterface.key() << iterInterface.value().interface;
            qInfo() << "---- permission:" << iterInterface.value().needPermission;
            qInfo() << "---- whitelist:" << iterInterface.value().processes;
            for (QMapMethod::iterator iterMethod = iterInterface.value().methods.begin();
                 iterMethod != iterInterface.value().methods.end(); iterMethod++) {
                qInfo() << "------ method:" << iterMethod.key() << iterMethod.value().method;
                qInfo() << "------ permission:" << iterMethod.value().needPermission;
                qInfo() << "------ whitelist:" << iterMethod.value().processes;
            }
            for (QMapProperty::iterator iterProp = iterInterface.value().properties.begin();
                 iterProp != iterInterface.value().properties.end(); iterProp++) {
                qInfo() << "------ property:" << iterProp.key() << iterProp.value().property;
                qInfo() << "------ permission:" << iterProp.value().needPermission;
                qInfo() << "------ whitelist:" << iterProp.value().processes;
            }
        }
    }
    qInfo() << "-------------------------------------";
}

void Policy::Test()
{
    qInfo() << "Policy::Test()";
    ParseConfig("plugin-sdbus-demo1.json");
    Print();
}

void Policy::ParseConfig(const QString &path)
{
    qInfo() << "[Policy]parse config:" << path;
    if (path.isEmpty()) {
        qInfo() << "[Policy]the serivce has not policy config.";
        return;
    }
    QJsonDocument jsonDoc;
    if( !readJsonFile(jsonDoc, path)) {
        qWarning() << "readJsonFile error..";
        return;
    }
    QJsonObject rootObj = jsonDoc.object();
    jsonGetString(rootObj, "name", m_name);
    jsonGetString(rootObj, "group", m_group);
    jsonGetString(rootObj, "libPath", m_libPath);
    jsonGetString(rootObj, "policyVersion", m_policyVersion);
    jsonGetString(rootObj, "policyStartType", m_policyStartType);
    if (m_name.isEmpty()) {
        qWarning() << "json error, name is empty";
        return;
    }
    if (!parseWhitelist(rootObj)) {
        qWarning() << "json error, parse Whitelist";
        return;
    }

    if (!parsePolicy(rootObj)) {
        qWarning() << "json error, parse policy";
        return;
    }
}

bool Policy::readJsonFile(QJsonDocument &outDoc, const QString &fileName)
{

    QFile jsonFIle(fileName);
    if( !jsonFIle.open(QIODevice::ReadOnly))
    {
        qWarning() << "open file jsonObject.json error!";
        return false;
    }

    QJsonParseError jsonParserError;
    outDoc = QJsonDocument::fromJson(jsonFIle.readAll(), &jsonParserError);
    jsonFIle.close();
    if (jsonParserError.error != QJsonParseError::NoError) {
        qWarning() << "fromJson jsonObject error!";
        return false;
    }
    if (outDoc.isNull()) {
        qWarning() << "readJsonFile error!";
        return false;
    }
    return true;
}

// typedef QMap<QString, PolicyWhitelist> QMapWhitelists;
bool Policy::parseWhitelist(const QJsonObject &obj)
{
    m_mapWhitelist.clear();
    if (!obj.contains("whitelists")) {
        // 为空，不是出错
        return true;
    }
    QJsonValue listsValue = obj.value("whitelists");
    if (!listsValue.isArray()) {
        qWarning() << "parse whitelist error, invalid format";
        return false;
    }
    QJsonArray lists = listsValue.toArray();
    for (int i = 0; i < lists.size(); ++i) {
        QJsonValue whitelistValue = lists.at(i);
        if (whitelistValue.isObject()) {
            PolicyWhitelist whitelist;
            QJsonObject whitelistObj = whitelistValue.toObject();
            QString name;
            jsonGetString(whitelistObj, "name", name);
            if (name.isEmpty()) {
                continue;
            }
            if (!whitelistObj.contains("process")) {
                continue;
            }
            QJsonArray processes = whitelistObj.value("process").toArray();
            if (processes.size() <= 0) {
                continue;
            }
            whitelist.name = name;
            for (int j = 0; j < processes.size(); ++j) {
                if (processes.at(j).isString()) {
                    whitelist.process.append(processes.at(j).toString());
                }
            }
            m_mapWhitelist.insert(name, whitelist);
        }
    }

    return true;
}

bool Policy::parsePolicy(const QJsonObject &obj)
{
    m_mapPathHide.clear();
    m_mapPath.clear();
    if (!obj.contains("policy")) {
        // 为空，不是出错
        return true;
    }
    QJsonValue policyValue = obj.value("policy");
    if (!policyValue.isArray()) {
        qWarning() << "parse policy error, invalid format";
        return false;
    }
    QJsonArray policyList = policyValue.toArray();
    for (int i = 0; i < policyList.size(); ++i) {
        QJsonValue policy = policyList.at(i);
        if (policy.isObject()) {
            bool ret = parsePolicyPath(policy.toObject());
            if (!ret) {
                return false;
            }
        }
    }
    return true;
}

bool Policy::parsePolicyPath(const QJsonObject &obj)
{
    QString path;
    jsonGetString(obj, "path", path);
    if (path.isEmpty()) {
        qWarning() << "parse policy path error, invalid format";
        return false;
    }

    bool pathHide;
    jsonGetBool(obj, "pathhide", pathHide, false);
    m_mapPathHide.insert(path, pathHide);

    bool subpath;
    jsonGetBool(obj, "subpath", subpath, false);
    m_mapSubPath.insert(path, pathHide);

    PolicyPath policyPath;
    policyPath.path = path;
    jsonGetBool(obj, "permission", policyPath.needPermission, false);
    QString whitelist;
    jsonGetString(obj, "whitelist", whitelist);
    if (!whitelist.isEmpty()) {
        QMapWhitelists::const_iterator iterWhitelist = m_mapWhitelist.find(whitelist);
        if (iterWhitelist != m_mapWhitelist.end() && iterWhitelist.value().name == whitelist) {
            policyPath.processes = iterWhitelist.value().process;
        }
    }

    if (obj.contains("interfaces")) {
        QJsonValue interfaces = obj.value("interfaces");
        if (interfaces.isArray()) {
            QJsonArray interfaceList = interfaces.toArray();
            for (int i = 0; i < interfaceList.size(); ++i) {
                QJsonValue interface = interfaceList.at(i);
                if (interface.isObject()) {
                    bool ret = parsePolicyInterface(interface.toObject(), policyPath);
                    if (!ret) {
                        return false;
                    }
                }
            }
        }
    }

    m_mapPath.insert(path, policyPath);

    return true;
}

bool Policy::parsePolicyInterface(const QJsonObject &obj, PolicyPath &policyPath)
{
    QString interface;
    jsonGetString(obj, "interface", interface);
    if (interface.isEmpty()) {
        qWarning() << "parse policy interface error, invalid format";
        return false;
    }

    PolicyInterface policyInterface;
    policyInterface.interface = interface;
    // interface没有指定permission，则使用上级path的permission
    jsonGetBool(obj, "permission", policyInterface.needPermission, policyPath.needPermission);
    QString whitelist;
    jsonGetString(obj, "whitelist", whitelist);
    if (!whitelist.isEmpty()) {
        QMapWhitelists::const_iterator iterWhitelist = m_mapWhitelist.find(whitelist);
        if (iterWhitelist != m_mapWhitelist.end() && iterWhitelist.value().name == whitelist) {
            policyInterface.processes = iterWhitelist.value().process;
        } // esle 错误的whitelist认为是空值
    } else {
        // interface没有指定whitelist，则使用上级path的whitelist
        policyInterface.processes = policyPath.processes;
    }

    if (obj.contains("methods")) {
        QJsonValue methods = obj.value("methods");
        if (methods.isArray()) {
            QJsonArray methodList = methods.toArray();
            for (int i = 0; i < methodList.size(); ++i) {
                QJsonValue method = methodList.at(i);
                if (method.isObject()) {
                    bool ret = parsePolicyMethod(method.toObject(), policyInterface);
                    if (!ret) {
                        return false;
                    }
                }
            }
        }
    }

    if (obj.contains("properties")) {
        QJsonValue properties = obj.value("properties");
        if (properties.isArray()) {
            QJsonArray propertiesList = properties.toArray();
            for (int i = 0; i < propertiesList.size(); ++i) {
                QJsonValue property = propertiesList.at(i);
                if (property.isObject()) {
                    bool ret = parsePolicyProperties(property.toObject(), policyInterface);
                    if (!ret) {
                        return false;
                    }
                }
            }
        }
    }

    policyPath.interfaces.insert(interface, policyInterface);
    return true;
}

bool Policy::parsePolicyMethod(const QJsonObject &obj, PolicyInterface &policyInterface)
{
    QString method;
    jsonGetString(obj, "method", method);
    if (method.isEmpty()) {
        qWarning() << "parse policy method error, invalid format";
        return false;
    }

    PolicyMethod policyMethod;
    policyMethod.method = method;
    // method没有指定permission，则使用上级interface的permission
    jsonGetBool(obj, "permission", policyMethod.needPermission, policyInterface.needPermission);
    QString whitelist;
    jsonGetString(obj, "whitelist", whitelist);
    if (!whitelist.isEmpty()) {
        QMapWhitelists::const_iterator iterWhitelist = m_mapWhitelist.find(whitelist);
        if (iterWhitelist != m_mapWhitelist.end() && iterWhitelist.value().name == whitelist) {
            policyMethod.processes = iterWhitelist.value().process;
        }
    } else {
        // method没有指定whitelist，则使用上级interface的whitelist
        policyMethod.processes = policyInterface.processes;
    }

    policyInterface.methods.insert(method, policyMethod);
    return true;
}

bool Policy::parsePolicyProperties(const QJsonObject &obj, PolicyInterface &policyInterface)
{
    QString property;
    jsonGetString(obj, "property", property);
    if (property.isEmpty()) {
        qWarning() << "parse policy property error, invalid format";
        return false;
    }

    PolicyProperty policyproperty;
    policyproperty.property = property;
    jsonGetBool(obj, "permission", policyproperty.needPermission, policyInterface.needPermission);
    QString whitelist;
    jsonGetString(obj, "whitelist", whitelist);
    if (!whitelist.isEmpty()) {
        QMapWhitelists::const_iterator iterWhitelist = m_mapWhitelist.find(whitelist);
        if (iterWhitelist != m_mapWhitelist.end() && iterWhitelist.value().name == whitelist) {
            policyproperty.processes = iterWhitelist.value().process;
        }
    } else {
        policyproperty.processes = policyInterface.processes;
    }

    policyInterface.properties.insert(property, policyproperty);
    return true;
}

bool Policy::jsonGetString(const QJsonObject &obj, const QString &key, QString &value, QString defaultValue)
{
    if (obj.contains(key)) {
        QJsonValue v = obj.value(key);
        if (v.isString()) {
            value = v.toString();
            return true;
        }
    }
    value = defaultValue;
    return false;
}

bool Policy::jsonGetBool(const QJsonObject &obj, const QString &key, bool &value, bool defaultValue)
{
    if (obj.contains(key)) {
        QJsonValue v = obj.value(key);
        if (v.isBool()) {
            value = v.toBool();
            return true;
        }
    }
    value = defaultValue;
    return false;
}
