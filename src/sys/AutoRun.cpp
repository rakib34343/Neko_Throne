#include "include/sys/AutoRun.hpp"

#include <QApplication>
#include <QDir>

#include "include/global/Configs.hpp"

#ifdef Q_OS_WIN

#include <QSettings>

QString Windows_GenAutoRunString() {
    auto appPath = QApplication::applicationFilePath();
    appPath = "\"" + QDir::toNativeSeparators(appPath) + "\"";
    appPath += " -tray";
    return appPath;
}

void AutoRun_SetEnabled(bool enable) {
    // 以程序名称作为注册表中的键
    // 根据键获取对应的值（程序路径）
    auto appPath = QApplication::applicationFilePath();
    QFileInfo fInfo(appPath);
    QString name = fInfo.baseName();

    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

    if (enable) {
        settings.setValue(name, Windows_GenAutoRunString());
    } else {
        settings.remove(name);
    }
}

bool AutoRun_IsEnabled() {
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

    // 以程序名称作为注册表中的键
    // 根据键获取对应的值（程序路径）
    auto appPath = QApplication::applicationFilePath();
    QFileInfo fInfo(appPath);
    QString name = fInfo.baseName();

    return settings.value(name).toString() == Windows_GenAutoRunString();
}

#endif

#ifdef Q_OS_LINUX

#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QTextStream>

#define NEWLINE "\n"

//  launchatlogin.cpp
//  ShadowClash
//
//  Created by TheWanderingCoel on 2018/6/12.
//  Copyright © 2019 Coel Wu. All rights reserved.
//
QString getUserAutostartDir_private() {
    QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    config += QLatin1String("/autostart/");
    return config;
}

void AutoRun_SetEnabled(bool enable) {
    // From https://github.com/nextcloud/desktop/blob/master/src/common/utility_unix.cpp
    QString appName = QCoreApplication::applicationName();
    QString userAutoStartPath = getUserAutostartDir_private();
    QString desktopFileLocation = userAutoStartPath + appName + QLatin1String(".desktop");
    QStringList appCmdList;

    if (QProcessEnvironment::systemEnvironment().contains("APPIMAGE")) {
        appCmdList << QProcessEnvironment::systemEnvironment().value("APPIMAGE");
    } else {
        appCmdList << QApplication::applicationFilePath();
    }

    appCmdList << "-tray";

    if (Configs::dataStore->flag_use_appdata) {
        appCmdList << "-appdata";
    }

    if (enable) {
        if (!QDir().exists(userAutoStartPath) && !QDir().mkpath(userAutoStartPath)) {
            // qCWarning(lcUtility) << "Could not create autostart folder"
            // << userAutoStartPath;
            return;
        }

        QFile iniFile(desktopFileLocation);

        if (!iniFile.open(QIODevice::WriteOnly)) {
            // qCWarning(lcUtility) << "Could not write auto start entry" <<
            // desktopFileLocation;
            return;
        }

        QTextStream ts(&iniFile);
        ts << QLatin1String("[Desktop Entry]") << NEWLINE
           << QLatin1String("Name=") << appName << NEWLINE
           << QLatin1String("Exec=") << appCmdList.join(" ") << NEWLINE
           << QLatin1String("Terminal=") << "false" << NEWLINE
           << QLatin1String("Categories=") << "Network" << NEWLINE
           << QLatin1String("Type=") << "Application" << NEWLINE
           << QLatin1String("StartupNotify=") << "false" << NEWLINE
           << QLatin1String("X-GNOME-Autostart-enabled=") << "true" << NEWLINE;
        ts.flush();
        iniFile.close();
    } else {
        QFile::remove(desktopFileLocation);
    }
}

bool AutoRun_IsEnabled() {
    QString appName = QCoreApplication::applicationName();
    QString desktopFileLocation = getUserAutostartDir_private() + appName + QLatin1String(".desktop");
    return QFile::exists(desktopFileLocation);
}

#endif
