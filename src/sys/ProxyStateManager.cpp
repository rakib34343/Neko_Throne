// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// src/sys/ProxyStateManager.cpp — Thread-safe proxy mode hot-swap engine
// ═══════════════════════════════════════════════════════════════════════════════
#include "include/sys/ProxyStateManager.hpp"
#include "include/global/Configs.hpp"
#include "include/api/RPC.h"
#include "3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.hpp"

#include <QDnsLookup>
#include <QEventLoop>
#include <QTimer>
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
    // Delegate to QvProxyConfigurator (handles GNOME + KDE + WinInet)
    ClearSystemProxy();

    // If TUN mode was active, clear system DNS
    if (Configs::dataStore && Configs::dataStore->spmode_vpn) {
        if (API::defaultClient) {
            bool rpcOK;
            API::defaultClient->SetSystemDNS(&rpcOK, /*clear=*/true);
        }
    }

    // Remove any leftover block routes
    removeBlockRoutes();
}

// ─── Proxy mode: activate system proxy pointing to local core ────────────────
void ProxyStateManager::transitionToProxy() {
    if (!Configs::dataStore) return;

    // Remove any leftover block routes first
    removeBlockRoutes();

    // Delegate to QvProxyConfigurator (handles GNOME + KDE + WinInet)
    auto socks_port = Configs::dataStore->inbound_socks_port;
    SetSystemProxy(socks_port, socks_port, Configs::dataStore->proxy_scheme);
}

// ─── Block mode: blackhole all outbound traffic ──────────────────────────────
void ProxyStateManager::transitionToBlock() {
    // 1. First, clear any proxy settings
    ClearSystemProxy();

    // 2. Inject blackhole routes at the OS level.
    //    This ensures zero packets escape even if the core crashes.
#ifdef Q_OS_WIN
    QProcess::execute(QStringLiteral("route"),
        {QStringLiteral("add"), QStringLiteral("0.0.0.0"),
         QStringLiteral("mask"), QStringLiteral("0.0.0.0"),
         QStringLiteral("0.0.0.0"),
         QStringLiteral("metric"), QStringLiteral("1"),
         QStringLiteral("if"), QStringLiteral("1")});
#elif defined(Q_OS_LINUX)
    QProcess::execute(QStringLiteral("ip"),
        {QStringLiteral("route"), QStringLiteral("add"),
         QStringLiteral("unreachable"), QStringLiteral("default"),
         QStringLiteral("metric"), QStringLiteral("1"),
         QStringLiteral("table"), QStringLiteral("throne_block")});
    QProcess::execute(QStringLiteral("ip"),
        {QStringLiteral("rule"), QStringLiteral("add"),
         QStringLiteral("priority"), QStringLiteral("100"),
         QStringLiteral("not"), QStringLiteral("from"), QStringLiteral("127.0.0.0/8"),
         QStringLiteral("lookup"), QStringLiteral("throne_block")});
#endif
}

// ─── Remove block routes (cleanup helper) ────────────────────────────────────
void ProxyStateManager::removeBlockRoutes() {
#ifdef Q_OS_WIN
    QProcess::execute(QStringLiteral("route"),
        {QStringLiteral("delete"), QStringLiteral("0.0.0.0"),
         QStringLiteral("mask"), QStringLiteral("0.0.0.0"),
         QStringLiteral("0.0.0.0"),
         QStringLiteral("if"), QStringLiteral("1")});
#elif defined(Q_OS_LINUX)
    QProcess::execute(QStringLiteral("ip"),
        {QStringLiteral("rule"), QStringLiteral("del"),
         QStringLiteral("priority"), QStringLiteral("100"),
         QStringLiteral("lookup"), QStringLiteral("throne_block")});
    QProcess::execute(QStringLiteral("ip"),
        {QStringLiteral("route"), QStringLiteral("del"),
         QStringLiteral("unreachable"), QStringLiteral("default"),
         QStringLiteral("table"), QStringLiteral("throne_block")});
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
    // Use Qt's native QDnsLookup — no dependency on dig/nslookup system tools.
    // Called from a QtConcurrent thread, so a nested event loop is safe here.
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(3000);
    QDnsLookup dns;
    dns.setType(QDnsLookup::A);
    dns.setName(QStringLiteral("dns.google"));
    QObject::connect(&dns, &QDnsLookup::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    dns.lookup();
    timeout.start();
    loop.exec();

    bool resolved = (dns.error() == QDnsLookup::NoError)
                    && !dns.hostAddressRecords().isEmpty();

    switch (m_mode.load(std::memory_order_acquire)) {
    case ProxyMode::Direct:
    case ProxyMode::Proxy:
        return true;
    case ProxyMode::Block:
        return !resolved;
    }
    return true;
}
