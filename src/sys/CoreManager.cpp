#include "include/sys/CoreManager.hpp"
#include "include/api/CoreVersionParser.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Utils.hpp"
#include "include/global/HTTPRequestHelper.hpp"

#include <QApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent>

namespace Configs_sys {

    struct CoreManager::Impl {
        mutable QMutex mu;
        CoreInfo singbox;
        CoreInfo xray;

        Impl() {
            singbox.engine = CoreEngine::SingBox;
            xray.engine = CoreEngine::XrayCore;
        }
    };

    CoreManager *CoreManager::instance() {
        static CoreManager inst;
        return &inst;
    }

    CoreManager::CoreManager(QObject *parent) : QObject(parent), d(new Impl) {
        auto *vp = CoreVersionParser::instance();
        connect(vp, &CoreVersionParser::versionParsed, this, [this](const CoreVersionInfo &info) {
            QList<CoreInfo> cores;
            {
                QMutexLocker lock(&d->mu);
                d->singbox.version = info.singboxVersion;
                d->singbox.available = info.singboxAvailable;
                d->singbox.running = (info.singboxStatus == "running");
                d->singbox.binaryPath = coreBinaryPath();

                d->xray.version = info.xrayVersion;
                d->xray.available = info.xrayAvailable;
                d->xray.running = (info.xrayStatus == "running");
                // Both sing-box and Xray-core are embedded in the same unified
                // Go backend binary — binaryPath is intentionally identical for both.
                d->xray.binaryPath = coreBinaryPath();

                cores = {d->singbox, d->xray};
            }
            emit coreInfoUpdated(cores);
        });
    }

    CoreManager::~CoreManager() {
        delete d;
    }

    CoreInfo CoreManager::info(CoreEngine engine) const {
        QMutexLocker lock(&d->mu);
        return engine == CoreEngine::SingBox ? d->singbox : d->xray;
    }

    QList<CoreInfo> CoreManager::allCores() const {
        QMutexLocker lock(&d->mu);
        return {d->singbox, d->xray};
    }

    QString CoreManager::coreBinaryPath() const {
        return Configs::FindCoreRealPath();
    }

    bool CoreManager::isCoreAvailable() const {
        return QFileInfo::exists(coreBinaryPath());
    }

    void CoreManager::refreshVersions() {
        CoreVersionParser::instance()->requestVersions();
    }

    void CoreManager::downloadLatestCore(std::function<void(bool, const QString &)> callback) {
        (void) QtConcurrent::run([this, cb = std::move(callback)] {
            QString platform;
#if defined(Q_OS_WIN)
            platform = "windows-amd64";
#elif defined(Q_OS_LINUX)
            platform = "linux-amd64";
#endif
            // Fetch latest release info from the correct upstream repository
            auto resp = Configs_network::NetworkRequestHelper::HttpGet(
                "https://api.github.com/repos/rakib34343/Neko_Throne/releases/latest");

            if (!resp.error.isEmpty()) {
                QMetaObject::invokeMethod(this, [cb, err = resp.error] {
                    cb(false, "Failed to check for updates: " + err);
                });
                return;
            }

            auto doc = QJsonDocument::fromJson(resp.data);
            if (doc.isNull()) {
                QMetaObject::invokeMethod(this, [cb] {
                    cb(false, "Invalid response from GitHub API");
                });
                return;
            }

            // Find the Core asset for our platform
            QString downloadUrl;
            auto assets = doc.object()["assets"].toArray();
            for (const auto &asset : assets) {
                auto name = asset.toObject()["name"].toString();
                if (name.contains("Core") && name.contains(platform)) {
                    downloadUrl = asset.toObject()["browser_download_url"].toString();
                    break;
                }
            }

            if (downloadUrl.isEmpty()) {
                QMetaObject::invokeMethod(this, [cb, platform] {
                    cb(false, "No Core binary found for platform: " + platform);
                });
                return;
            }

            auto destPath = coreBinaryPath();
            auto tempPath = destPath + ".update";

            auto dlError = Configs_network::NetworkRequestHelper::DownloadAsset(downloadUrl, tempPath);
            if (!dlError.isEmpty()) {
                QFile::remove(tempPath);
                QString dlErr = dlError;
                QMetaObject::invokeMethod(this, [cb, dlErr] {
                    cb(false, "Download failed: " + dlErr);
                });
                return;
            }

            // Replace old binary
            QFile::remove(destPath + ".bak");
            QFile::rename(destPath, destPath + ".bak");
            if (!QFile::rename(tempPath, destPath)) {
                QFile::rename(destPath + ".bak", destPath);
                QMetaObject::invokeMethod(this, [cb] {
                    cb(false, "Failed to replace Core binary");
                });
                return;
            }

#ifndef Q_OS_WIN
            QFile::setPermissions(destPath,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                QFileDevice::ReadOther | QFileDevice::ExeOther);
#endif

            QMetaObject::invokeMethod(this, [this, cb] {
                refreshVersions();
                cb(true, QString());
            });
        });
    }

} // namespace Configs_sys
