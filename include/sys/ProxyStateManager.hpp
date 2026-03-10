// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// include/sys/ProxyStateManager.hpp — Thread-safe, zero-restart proxy mode
// hot-swap engine.
//
// Manages three proxy states: Direct, Proxy, Block.
// Transitions happen atomically — no connection drop, no process restart.
// ═══════════════════════════════════════════════════════════════════════════════
#pragma once

#include <QObject>
#include <atomic>

// ─── Proxy operating modes ───────────────────────────────────────────────────
enum class ProxyMode : int {
    Direct = 0,   // All traffic flows directly (bypass proxy)
    Proxy  = 1,   // All traffic routed through active proxy core
    Block  = 2    // All outbound traffic blocked (kill switch active)
};

// ═══════════════════════════════════════════════════════════════════════════════
// ProxyStateManager — Singleton, thread-safe
// ═══════════════════════════════════════════════════════════════════════════════
//
// Design principles:
//   1. State transitions are serialized via a QMutex to prevent races.
//   2. setMode() is non-blocking from the caller's perspective — actual
//      work (OS routing table changes, core reload) happens on a worker
//      thread behind the scenes.
//   3. Active local connections are NOT dropped during transitions.
//      For Direct↔Proxy, we reconfigure the routing/TUN without tearing
//      down the core process. For Block mode, we inject a blackhole route.
//   4. Every transition validates post-conditions (routing table, DNS)
//      and emits modeChangeVerified() with diagnostic details.
//
class ProxyStateManager : public QObject {
    Q_OBJECT

public:
    static ProxyStateManager *instance();

    // ── Current state (lock-free read) ────────────────────────────
    ProxyMode currentMode() const { return m_mode.load(std::memory_order_acquire); }

    // ── Request a mode transition (non-blocking) ─────────────────
    // Returns false only if a transition is already in progress.
    bool setMode(ProxyMode target);

    // ── Kill switch: instant Block with OS-level enforcement ─────
    // Called on connection drop or user panic button.
    void activateKillSwitch();
    void deactivateKillSwitch();

    // ── Query ────────────────────────────────────────────────────
    bool isTransitioning() const { return m_transitioning.load(std::memory_order_acquire); }
    bool isKillSwitchActive() const { return m_killSwitch.load(std::memory_order_acquire); }

signals:
    // Emitted after mode change completes successfully.
    void modeChanged(ProxyMode newMode);

    // Emitted after post-transition validation.
    void modeChangeVerified(ProxyMode mode, bool routingOK, bool dnsOK);

    // Emitted if transition fails (mode reverts to previous).
    void modeChangeFailed(ProxyMode attempted, const QString &reason);

    // Kill switch state changed.
    void killSwitchStateChanged(bool active);

private:
    explicit ProxyStateManager(QObject *parent = nullptr);
    ~ProxyStateManager() override = default;

    Q_DISABLE_COPY_MOVE(ProxyStateManager)

    // Internal transition workers (run on worker thread)
    void transitionToDirect();
    void transitionToProxy();
    void transitionToBlock();
    void removeBlockRoutes();

    // Post-transition validation
    bool validateRoutingTable();
    bool validateDNSResolution();

    std::atomic<ProxyMode> m_mode{ProxyMode::Direct};
    std::atomic<bool> m_transitioning{false};
    std::atomic<bool> m_killSwitch{false};
};
