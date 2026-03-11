#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

namespace Configs_sys {

    // Core engine types supported by the unified backend
    enum class CoreEngine {
        SingBox,    // Always required — primary routing engine
        XrayCore,   // Optional — used for VLESS/XTLS protocols
    };

    struct CoreInfo {
        CoreEngine engine;
        QString version;
        QString binaryPath;
        bool available = false;
        bool running = false;
    };

    // ═══════════════════════════════════════════════════════════════════════
    // CoreManager — manages discovery, health, and updates for core binaries.
    //
    // The unified Core binary (Go backend) embeds both sing-box and Xray-core.
    // This class tracks their status and provides update capabilities.
    // ═══════════════════════════════════════════════════════════════════════
    class CoreManager : public QObject {
        Q_OBJECT

    public:
        static CoreManager *instance();

        // Returns info for a specific engine
        CoreInfo info(CoreEngine engine) const;

        // Returns all available engine infos
        QList<CoreInfo> allCores() const;

        // Path to the unified Core binary
        QString coreBinaryPath() const;

        // Check if the Core binary exists and is executable
        bool isCoreAvailable() const;

        // Trigger async version refresh (calls CoreVersionParser internally)
        void refreshVersions();

        // Download or update Core from GitHub releases
        // Callback receives (success, errorMessage)
        void downloadLatestCore(std::function<void(bool, const QString &)> callback);

    signals:
        void coreInfoUpdated(const QList<CoreInfo> &cores);
        void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    private:
        explicit CoreManager(QObject *parent = nullptr);
        ~CoreManager() override;
        Q_DISABLE_COPY_MOVE(CoreManager)

        struct Impl;
        Impl *d;
    };

} // namespace Configs_sys
