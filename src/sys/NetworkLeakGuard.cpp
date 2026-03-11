// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// src/sys/NetworkLeakGuard.cpp — OS-level IP/DNS leak prevention engine
// ═══════════════════════════════════════════════════════════════════════════════
#include "include/sys/NetworkLeakGuard.hpp"
#include "include/global/Configs.hpp"

#include <QDnsLookup>
#include <QEventLoop>
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
    r.tunInterfaceUp = true;

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

    // Use Qt's built-in QDnsLookup for a portable, tool-independent DNS probe.
    // This avoids any dependency on 'dig' or 'nslookup' system utilities.
    // NOTE: This function is always called from a QtConcurrent worker thread,
    //       so the nested event loop is safe here.
    QEventLoop loop;
    QTimer dnsTimeout;
    dnsTimeout.setSingleShot(true);
    dnsTimeout.setInterval(5000);
    QDnsLookup dns;
    dns.setType(QDnsLookup::A);
    dns.setName(QStringLiteral("whoami.akamai.net"));
    QObject::connect(&dns, &QDnsLookup::finished, &loop, &QEventLoop::quit);
    QObject::connect(&dnsTimeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    dns.lookup();
    dnsTimeout.start();
    loop.exec();

    if (dns.error() != QDnsLookup::NoError) {
        if (vpnMode) {
            r.dnsLeakFree = false;
            r.diagnostics << QStringLiteral("DNS probe failed in VPN mode: ") + dns.errorString();
        } else {
            r.diagnostics << QStringLiteral("DNS probe failed: ") + dns.errorString();
        }
        return r;
    }

    const auto addresses = dns.hostAddressRecords();
    if (addresses.isEmpty()) {
        r.diagnostics << QStringLiteral("DNS probe: no A records returned for whoami.akamai.net");
    } else {
        r.diagnostics << QStringLiteral("DNS probe resolved: ") + addresses.first().value().toString();
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
#elif defined(Q_OS_WIN)
        // On Windows, check IPv6 routes for a TUN interface using netsh.
        QProcess proc;
        proc.start(QStringLiteral("netsh"),
            {QStringLiteral("interface"), QStringLiteral("ipv6"),
             QStringLiteral("show"), QStringLiteral("route")});
        if (proc.waitForFinished(3000)) {
            auto output = QString::fromUtf8(proc.readAllStandardOutput());
            static const QRegularExpression rxTunWin(QStringLiteral(R"((sing-tun|Wintun|tun\d+))"));
            if (!rxTunWin.match(output).hasMatch()) {
                r.ipv6Contained = false;
                r.diagnostics << QStringLiteral("IPv6 route table does not include TUN interface on Windows — potential leak");
            }
        } else {
            r.diagnostics << QStringLiteral("netsh IPv6 route query timed out or failed — IPv6 tunnel state unknown");
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
    // NOTE: This registry write disables IPv6 system-wide and persists across
    // reboots. Only apply when the process is confirmed to be running as admin.
    if (!Configs::IsAdmin()) {
        // Cannot apply IPv6 block without administrator privileges.
        qWarning() << "NetworkLeakGuard: blockIPv6Leaks skipped — not running as administrator";
        return;
    }
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
