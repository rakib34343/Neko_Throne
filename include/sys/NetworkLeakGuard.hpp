// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// include/sys/NetworkLeakGuard.hpp — OS-level IP/DNS leak prevention engine
// ═══════════════════════════════════════════════════════════════════════════════
// Guarantees zero-leak policy during:
//   - Kill switch activation (connection drop)
//   - TUN interface mode switching
//   - Proxy state transitions
//
// Architecture:
//   - Periodic OS routing table validation
//   - DNS leak detection via controlled probes
//   - WebRTC/IPv6 leak prevention hints
//   - Automatic remediation on drift detection
// ═══════════════════════════════════════════════════════════════════════════════
#pragma once

#include <QObject>
#include <QTimer>
#include <QStringList>
#include <atomic>

// ─── Leak detection result ───────────────────────────────────────────────────
struct LeakAuditResult {
    bool routingIntact = true;       // OS routing table matches expected state
    bool dnsLeakFree = true;         // No DNS queries bypass our resolver
    bool ipv6Contained = true;       // IPv6 traffic is not leaking
    bool tunInterfaceUp = true;      // TUN device is operational (if VPN mode)
    QStringList diagnostics;         // Human-readable diagnostic messages
};

class NetworkLeakGuard : public QObject {
    Q_OBJECT

public:
    static NetworkLeakGuard *instance();

    // ── Full audit — snapshot routing table + DNS + IPv6 ──
    void runFullAudit();

    // ── Start/stop periodic monitoring ──
    // intervalMs is clamped to a minimum of 15 000 ms (15 s) regardless of
    // the value passed in. Default is 30 s, which keeps background CPU usage
    // negligible while still catching routing-table drift in a reasonable time.
    void startMonitoring(int intervalMs = 30000);
    void stopMonitoring();

    // ── IPv6 leak prevention ──
    // Disables IPv6 at the OS level when VPN is active to prevent leaks.
    void blockIPv6Leaks();
    void restoreIPv6();

    // ── TUN interface health check ──
    bool isTunInterfaceUp() const;

signals:
    // Emitted after each audit cycle with results.
    void auditCompleted(const LeakAuditResult &result);

    // Emitted when a leak is detected — UI should react immediately.
    void leakDetected(const QString &type, const QString &detail);

private:
    explicit NetworkLeakGuard(QObject *parent = nullptr);
    ~NetworkLeakGuard() override = default;

    Q_DISABLE_COPY_MOVE(NetworkLeakGuard)

    // Platform-specific audit workers
    LeakAuditResult auditRoutingTable();
    LeakAuditResult auditDNSLeaks();
    LeakAuditResult auditIPv6();

    QTimer *m_timer = nullptr;
    std::atomic<bool> m_auditRunning{false};
};
