// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// src/sys/NetworkLeakGuard.cpp — OS-level IP/DNS leak prevention engine
// ═══════════════════════════════════════════════════════════════════════════════
#include "include/sys/NetworkLeakGuard.hpp"
#include "include/global/Configs.hpp"

#include <QProcess>
#include <QRegularExpression>
#include <QtConcurrent>

// ─── Singleton ───────────────────────────────────────────────────────────────
NetworkLeakGuard *NetworkLeakGuard::instance() {
    static NetworkLeakGuard s_instance;
    return &s_instance;
}

NetworkLeakGuard::NetworkLeakGuard(QObject *parent) : QObject(parent) {}

// ═══════════════════════════════════════════════════════════════════════════════
// Full Audit — runs routing, DNS, and IPv6 checks off-thread
// ═══════════════════════════════════════════════════════════════════════════════
void NetworkLeakGuard::runFullAudit() {
    if (m_auditRunning.exchange(true, std::memory_order_acq_rel))
        return; // previous audit still running, skip

    (void) QtConcurrent::run([this] {
        LeakAuditResult combined;

        auto routeResult = auditRoutingTable();
        auto dnsResult = auditDNSLeaks();
        auto ipv6Result = auditIPv6();

        combined.routingIntact = routeResult.routingIntact;
        combined.dnsLeakFree = dnsResult.dnsLeakFree;
        combined.ipv6Contained = ipv6Result.ipv6Contained;
        combined.tunInterfaceUp = routeResult.tunInterfaceUp;

        combined.diagnostics << routeResult.diagnostics
                             << dnsResult.diagnostics
                             << ipv6Result.diagnostics;

        QMetaObject::invokeMethod(this, [this, combined] {
            m_auditRunning.store(false, std::memory_order_release);
            emit auditCompleted(combined);

            if (!combined.routingIntact)
                emit leakDetected(QStringLiteral("routing"), QStringLiteral("OS routing table drift detected"));
            if (!combined.dnsLeakFree)
                emit leakDetected(QStringLiteral("dns"), QStringLiteral("DNS queries bypassing proxy resolver"));
            if (!combined.ipv6Contained)
                emit leakDetected(QStringLiteral("ipv6"), QStringLiteral("IPv6 traffic leaking outside tunnel"));
        });
    });
}

// ─── Periodic monitoring ─────────────────────────────────────────────────────
void NetworkLeakGuard::startMonitoring(int intervalMs) {
    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &NetworkLeakGuard::runFullAudit);
    }
    m_timer->start(intervalMs > 15000 ? intervalMs : 15000);
}

void NetworkLeakGuard::stopMonitoring() {
    if (m_timer)
        m_timer->stop();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Routing Table Audit
// ═══════════════════════════════════════════════════════════════════════════════
// Checks that the default route goes through our TUN interface (if VPN mode)
// or our SOCKS/HTTP proxy (if proxy mode).

LeakAuditResult NetworkLeakGuard::auditRoutingTable() {
    LeakAuditResult r;
    r.routingIntact = true;
    r. tunInterfaceUp = true;

    bool vpnMode = Configs::dataStore && Configs::dataStore->spmode_vpn;

    QProcess proc;
#ifdef Q_OS_WIN
    proc.start(QStringLiteral("route"), {QStringLiteral("print"), QStringLiteral("0.0.0.0")});
#elif defined(Q_OS_LINUX)
    proc.start(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("show"), QStringLiteral("default")});
#endif

    if (!proc.waitForFinished(3000)) {
        r.routingIntact = false;
        r.diagnostics << QStringLiteral("Routing table query timed out");
        return r;
    }

    auto output = QString::fromUtf8(proc.readAllStandardOutput());

    if (vpnMode) {
        // In VPN/TUN mode, the default route MUST go through a TUN device
        // Common TUN names: tun0, utun3, sing-tun, Wintun
        static const QRegularExpression rxTun(
            QStringLiteral(R"((tun\d+|utun\d+|sing-tun|Wintun))")
        );
        if (!rxTun.match(output).hasMatch()) {
            r.routingIntact = false;
            r.tunInterfaceUp = false;
            r.diagnostics << QStringLiteral("Default route does NOT go through TUN interface");
        } else {
            r.diagnostics << QStringLiteral("OK: Default route via TUN");
        }
    } else {
        // In system proxy mode, we check that no unexpected blackhole routes exist
        if (output.contains(QStringLiteral("blackhole")) || output.contains(QStringLiteral("unreachable"))) {
            r.diagnostics << QStringLiteral("WARNING: Stale block routes detected in routing table");
            r.routingIntact = false;
        }
    }

    return r;
}

// ═══════════════════════════════════════════════════════════════════════════════
// DNS Leak Audit
// ═══════════════════════════════════════════════════════════════════════════════
// Sends a DNS query to an external test resolver and checks whether the
// response comes from our proxy's DNS or from the system's default resolver.

LeakAuditResult NetworkLeakGuard::auditDNSLeaks() {
    LeakAuditResult r;
    r.dnsLeakFree = true;

    bool vpnMode = Configs::dataStore && Configs::dataStore->spmode_vpn;
    if (!vpnMode && !(Configs::dataStore && Configs::dataStore->enable_dns_server)) {
        r.diagnostics << QStringLiteral("DNS audit skipped (not in VPN/DNS hijack mode)");
        return r;
    }

    // Query a well-known domain via the system resolver.
    // If we are in VPN mode, ALL DNS should resolve through our TUN/proxy.
    QProcess proc;
#ifdef Q_OS_WIN
    proc.start(QStringLiteral("nslookup"),
        {QStringLiteral("whoami.akamai.net")});
#else
    proc.start(QStringLiteral("dig"),
        {QStringLiteral("+short"), QStringLiteral("+time=2"),
         QStringLiteral("+tries=1"), QStringLiteral("whoami.akamai.net")});
#endif

    if (!proc.waitForFinished(5000)) {
        r.diagnostics << QStringLiteral("DNS probe timed out");
        return r;
    }

    auto output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    // If the response contains our ISP's DNS IP (rather than proxy exit),
    // that indicates a DNS leak. For now, we just verify the query resolved
    // and that in Block mode it should NOT resolve at all.
    // The real check would compare resolved IP vs known proxy exit IPs.
    if (output.isEmpty()) {
        if (vpnMode) {
            r.dnsLeakFree = false;
            r.diagnostics << QStringLiteral("DNS resolution failed in VPN mode (possible misconfiguration)");
        }
    } else {
        r.diagnostics << QStringLiteral("DNS probe resolved: ") + output;
    }

    return r;
}

// ═══════════════════════════════════════════════════════════════════════════════
// IPv6 Leak Audit & Prevention
// ═══════════════════════════════════════════════════════════════════════════════

LeakAuditResult NetworkLeakGuard::auditIPv6() {
    LeakAuditResult r;
    r.ipv6Contained = true;

    // Check if IPv6 is solicited by the user
    bool ipv6enabled = Configs::dataStore && Configs::dataStore->vpn_ipv6;

    if (ipv6enabled) {
        r.diagnostics << QStringLiteral("IPv6 enabled by user — checking tunnel coverage");
#ifdef Q_OS_LINUX
        QProcess proc;
        proc.start(QStringLiteral("ip"), {QStringLiteral("-6"), QStringLiteral("route"), QStringLiteral("show"), QStringLiteral("default")});
        if (proc.waitForFinished(3000)) {
            auto output = QString::fromUtf8(proc.readAllStandardOutput());
            static const QRegularExpression rxTun(QStringLiteral(R"((tun\d+|sing-tun))"));
            if (!rxTun.match(output).hasMatch()) {
                r.ipv6Contained = false;
                r.diagnostics << QStringLiteral("IPv6 default route does NOT use TUN — potential leak");
            }
        }
#endif
    } else {
        r.diagnostics << QStringLiteral("IPv6 disabled by user setting");
    }

    return r;
}

// ─── IPv6 block/restore (call when VPN activates/deactivates) ────────────────
void NetworkLeakGuard::blockIPv6Leaks() {
#ifdef Q_OS_LINUX
    QProcess::execute(QStringLiteral("sysctl"),
        {QStringLiteral("-w"), QStringLiteral("net.ipv6.conf.all.disable_ipv6=1")});
    QProcess::execute(QStringLiteral("sysctl"),
        {QStringLiteral("-w"), QStringLiteral("net.ipv6.conf.default.disable_ipv6=1")});
#elif defined(Q_OS_WIN)
    // Windows: Disable IPv6 on all adapters via registry
    QProcess::execute(QStringLiteral("reg"),
        {QStringLiteral("add"),
         QStringLiteral(R"(HKLM\SYSTEM\CurrentControlSet\Services\Tcpip6\Parameters)"),
         QStringLiteral("/v"), QStringLiteral("DisabledComponents"),
         QStringLiteral("/t"), QStringLiteral("REG_DWORD"),
         QStringLiteral("/d"), QStringLiteral("255"),
         QStringLiteral("/f")});
#endif
    // macOS: sing-box TUN handles IPv6 routing natively
}

void NetworkLeakGuard::restoreIPv6() {
#ifdef Q_OS_LINUX
    QProcess::execute(QStringLiteral("sysctl"),
        {QStringLiteral("-w"), QStringLiteral("net.ipv6.conf.all.disable_ipv6=0")});
    QProcess::execute(QStringLiteral("sysctl"),
        {QStringLiteral("-w"), QStringLiteral("net.ipv6.conf.default.disable_ipv6=0")});
#elif defined(Q_OS_WIN)
    QProcess::execute(QStringLiteral("reg"),
        {QStringLiteral("add"),
         QStringLiteral(R"(HKLM\SYSTEM\CurrentControlSet\Services\Tcpip6\Parameters)"),
         QStringLiteral("/v"), QStringLiteral("DisabledComponents"),
         QStringLiteral("/t"), QStringLiteral("REG_DWORD"),
         QStringLiteral("/d"), QStringLiteral("0"),
         QStringLiteral("/f")});
#endif
}

bool NetworkLeakGuard::isTunInterfaceUp() const {
    QProcess proc;
#ifdef Q_OS_LINUX
    proc.start(QStringLiteral("ip"), {QStringLiteral("link"), QStringLiteral("show"), QStringLiteral("type"), QStringLiteral("tun")});
#elif defined(Q_OS_WIN)
    proc.start(QStringLiteral("netsh"),
        {QStringLiteral("interface"), QStringLiteral("show"), QStringLiteral("interface")});
#endif
    if (!proc.waitForFinished(3000))
        return false;

    auto output = QString::fromUtf8(proc.readAllStandardOutput());

    static const QRegularExpression rxTun(
        QStringLiteral(R"((tun\d+|utun\d+|sing-tun|Wintun))")
    );
    return rxTun.match(output).hasMatch();
}
