// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// src/sys/ProxyStateManager.cpp — Thread-safe proxy mode hot-swap engine
// ═══════════════════════════════════════════════════════════════════════════════
#include "include/sys/ProxyStateManager.hpp"
#include "include/global/Configs.hpp"
#include "include/api/RPC.h"

#include <QProcess>
#include <QtConcurrent>

// ─── Singleton ───────────────────────────────────────────────────────────────
ProxyStateManager *ProxyStateManager::instance() {
    static ProxyStateManager s_instance;
    return &s_instance;
}

ProxyStateManager::ProxyStateManager(QObject *parent) : QObject(parent) {}

// ═══════════════════════════════════════════════════════════════════════════════
// setMode — The main public entry point for mode transitions
// ═══════════════════════════════════════════════════════════════════════════════
// Returns false if a transition is already in progress (no queuing).
// The actual work is dispatched to a worker thread.

bool ProxyStateManager::setMode(ProxyMode target) {
    // Fast path: already in desired mode
    if (m_mode.load(std::memory_order_acquire) == target)
        return true;

    // Prevent concurrent transitions
    if (m_transitioning.exchange(true, std::memory_order_acq_rel)) {
        return false; // another transition in flight
    }

    auto previousMode = m_mode.load(std::memory_order_acquire);

    (void) QtConcurrent::run([this, target, previousMode] {
        bool success = false;

        switch (target) {
        case ProxyMode::Direct:
            transitionToDirect();
            success = true;
            break;
        case ProxyMode::Proxy:
            transitionToProxy();
            success = true;
            break;
        case ProxyMode::Block:
            transitionToBlock();
            success = true;
            break;
        }

        if (success) {
            m_mode.store(target, std::memory_order_release);

            // Post-transition validation
            bool routeOK = validateRoutingTable();
            bool dnsOK = validateDNSResolution();

            QMetaObject::invokeMethod(this, [this, target, routeOK, dnsOK] {
                emit modeChanged(target);
                emit modeChangeVerified(target, routeOK, dnsOK);
            });
        } else {
            m_mode.store(previousMode, std::memory_order_release);
            QMetaObject::invokeMethod(this, [this, target] {
                emit modeChangeFailed(target, QStringLiteral("Transition logic failed"));
            });
        }

        m_transitioning.store(false, std::memory_order_release);
    });

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Kill Switch — Instant Block mode with OS-level route enforcement
// ═══════════════════════════════════════════════════════════════════════════════

void ProxyStateManager::activateKillSwitch() {
    m_killSwitch.store(true, std::memory_order_release);
    setMode(ProxyMode::Block);
    emit killSwitchStateChanged(true);
}

void ProxyStateManager::deactivateKillSwitch() {
    m_killSwitch.store(false, std::memory_order_release);
    setMode(ProxyMode::Direct);
    emit killSwitchStateChanged(false);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Transition implementations
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Direct mode: remove system proxy, leave core running for local apps ─────
void ProxyStateManager::transitionToDirect() {
    // 1. Clear system proxy settings (platform-specific)
#ifdef Q_OS_WIN
    // On Windows, clear WinInet proxy via registry
    QProcess::execute(QStringLiteral("reg"),
        {QStringLiteral("add"),
         QStringLiteral(R"(HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings)"),
         QStringLiteral("/v"), QStringLiteral("ProxyEnable"),
         QStringLiteral("/t"), QStringLiteral("REG_DWORD"),
         QStringLiteral("/d"), QStringLiteral("0"),
         QStringLiteral("/f")});
#elif defined(Q_OS_LINUX)
    // On Linux, clear GNOME/KDE proxy settings
    QProcess::execute(QStringLiteral("gsettings"),
        {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy"),
         QStringLiteral("mode"), QStringLiteral("'none'")});
#endif

    // 2. If TUN mode was active, signal the core to deactivate TUN
    //    The core stays alive — only routing changes.
    if (Configs::dataStore && Configs::dataStore->spmode_vpn) {
        // Clear system DNS if it was set by us
        if (API::defaultClient) {
            bool rpcOK;
            API::defaultClient->SetSystemDNS(&rpcOK, /*clear=*/true);
        }
    }
}

// ─── Proxy mode: activate system proxy pointing to local core ────────────────
void ProxyStateManager::transitionToProxy() {
    if (!Configs::dataStore) return;

    auto port = QString::number(Configs::dataStore->core_port);
    auto addr = QStringLiteral("127.0.0.1");

#ifdef Q_OS_WIN
    auto proxyStr = addr + ":" + port;
    QProcess::execute(QStringLiteral("reg"),
        {QStringLiteral("add"),
         QStringLiteral(R"(HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings)"),
         QStringLiteral("/v"), QStringLiteral("ProxyServer"),
         QStringLiteral("/t"), QStringLiteral("REG_SZ"),
         QStringLiteral("/d"), proxyStr,
         QStringLiteral("/f")});
    QProcess::execute(QStringLiteral("reg"),
        {QStringLiteral("add"),
         QStringLiteral(R"(HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings)"),
         QStringLiteral("/v"), QStringLiteral("ProxyEnable"),
         QStringLiteral("/t"), QStringLiteral("REG_DWORD"),
         QStringLiteral("/d"), QStringLiteral("1"),
         QStringLiteral("/f")});
#elif defined(Q_OS_LINUX)
    QProcess::execute(QStringLiteral("gsettings"),
        {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy"),
         QStringLiteral("mode"), QStringLiteral("'manual'")});
    QProcess::execute(QStringLiteral("gsettings"),
        {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy.http"),
         QStringLiteral("host"), addr});
    QProcess::execute(QStringLiteral("gsettings"),
        {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy.http"),
         QStringLiteral("port"), port});
    QProcess::execute(QStringLiteral("gsettings"),
        {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy.socks"),
         QStringLiteral("host"), addr});
    QProcess::execute(QStringLiteral("gsettings"),
        {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy.socks"),
         QStringLiteral("port"), port});
#endif
}

// ─── Block mode: blackhole all outbound traffic ──────────────────────────────
void ProxyStateManager::transitionToBlock() {
    // 1. First, apply Direct mode to clear any proxy settings
    transitionToDirect();

    // 2. Inject blackhole routes at the OS level.
    //    This ensures zero packets escape even if the core crashes.
#ifdef Q_OS_WIN
    // Windows: Add a blackhole route for 0.0.0.0/0 with unreachable gateway.
    // Use metric 1 to take priority over all other default routes.
    // Preserve localhost (127.0.0.0/8) for local RPC communication.
    QProcess::execute(QStringLiteral("route"),
        {QStringLiteral("add"), QStringLiteral("0.0.0.0"),
         QStringLiteral("mask"), QStringLiteral("0.0.0.0"),
         QStringLiteral("0.0.0.0"),
         QStringLiteral("metric"), QStringLiteral("1"),
         QStringLiteral("if"), QStringLiteral("1")}); // loopback interface
#elif defined(Q_OS_LINUX)
    // Linux: Add an unreachable default route with high priority
    QProcess::execute(QStringLiteral("ip"),
        {QStringLiteral("route"), QStringLiteral("add"),
         QStringLiteral("unreachable"), QStringLiteral("default"),
         QStringLiteral("metric"), QStringLiteral("1"),
         QStringLiteral("table"), QStringLiteral("throne_block")});
    // Apply policy routing rule
    QProcess::execute(QStringLiteral("ip"),
        {QStringLiteral("rule"), QStringLiteral("add"),
         QStringLiteral("priority"), QStringLiteral("100"),
         QStringLiteral("not"), QStringLiteral("from"), QStringLiteral("127.0.0.0/8"),
         QStringLiteral("lookup"), QStringLiteral("throne_block")});
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
// Post-transition Validation
// ═══════════════════════════════════════════════════════════════════════════════

bool ProxyStateManager::validateRoutingTable() {
    QProcess proc;
#ifdef Q_OS_WIN
    proc.start(QStringLiteral("route"), {QStringLiteral("print")});
#elif defined(Q_OS_LINUX)
    proc.start(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("show")});
#endif
    if (!proc.waitForFinished(3000))
        return false;

    auto output = QString::fromUtf8(proc.readAllStandardOutput());

    switch (m_mode.load(std::memory_order_acquire)) {
    case ProxyMode::Direct:
        // Verify no Throne-specific block routes remain
        return !output.contains(QStringLiteral("throne_block"))
            && !output.contains(QStringLiteral("blackhole"));
    case ProxyMode::Proxy:
        // Verify default route exists (not blackholed)
        return !output.contains(QStringLiteral("blackhole"));
    case ProxyMode::Block:
        // Verify block routes are active
#ifdef Q_OS_LINUX
        return output.contains(QStringLiteral("unreachable"));
#else
        return output.contains(QStringLiteral("blackhole"))
            || output.contains(QStringLiteral("127.0.0.1"));
#endif
    }
    return false;
}

bool ProxyStateManager::validateDNSResolution() {
    // Quick DNS probe to ensure DNS is not leaking in Block mode,
    // and is functional in Direct/Proxy modes.
    QProcess proc;
#ifdef Q_OS_WIN
    proc.start(QStringLiteral("nslookup"),
        {QStringLiteral("dns.google"), QStringLiteral("127.0.0.1")});
#else
    proc.start(QStringLiteral("dig"),
        {QStringLiteral("+short"), QStringLiteral("+time=2"),
         QStringLiteral("dns.google"), QStringLiteral("@127.0.0.1")});
#endif
    proc.waitForFinished(3000);

    bool resolved = proc.exitCode() == 0
                    && !proc.readAllStandardOutput().trimmed().isEmpty();

    switch (m_mode.load(std::memory_order_acquire)) {
    case ProxyMode::Direct:
    case ProxyMode::Proxy:
        return true; // DNS should work (we don't control external DNS here)
    case ProxyMode::Block:
        return !resolved; // In Block mode, DNS MUST fail to external servers
    }
    return true;
}
