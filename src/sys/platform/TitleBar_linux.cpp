// SPDX-License-Identifier: GPL-2.0-or-later
// TitleBar platform helpers — Linux implementation
// Handles Wayland CSD, X11 frameless fallback.

#if !defined(_WIN32)

#include "include/ui/core/TitleBar.hpp"

#include <QGuiApplication>
#include <QWidget>
#include <QWindow>

namespace Platform {

bool isMicaSupported() {
    return false;
}

void enableMicaEffect(QWidget *) {
    // No-op on Linux.
}

bool isWayland() {
    return QGuiApplication::platformName() == QStringLiteral("wayland");
}

void configureLinuxCSD(QWidget *window) {
    if (!window)
        return;

    // On Wayland (GNOME/KDE) the compositor draws server-side decorations by default
    // when the window has standard flags. We do NOT set Qt::FramelessWindowHint here
    // because that strips the CSD entirely — losing resize handles and compositor shadows.
    //
    // Instead we keep the native frame and place our custom TitleBar inside the client area.
    // The compositor's CSD (close/min/max buttons) will overlay on GNOME; on KDE Plasma
    // it depends on the theme.
    //
    // For a fully custom look, set the Qt::FramelessWindowHint and use
    // _GTK_FRAME_EXTENTS atoms (X11) or xdg-decoration protocol (Wayland).
    // This is left as opt-in because not all compositors handle it well.

    // Hint to Wayland compositors that we'd prefer server-side decorations
    // (SSD) so that our window fits natively in the desktop.
    window->setProperty("_q_waylandPreferServerSideDecorations", true);

    // On X11 with KDE, set the _KDE_NET_WM_BLUR_BEHIND_REGION for translucent backgrounds
    // (similar to Acrylic). This is best-effort.
}

void installSnapLayoutFilter(QWidget *, TitleBar *) {
    // No-op on Linux — Snap Layouts are a Windows 11 feature.
}

}  // namespace Platform

#endif // !_WIN32
