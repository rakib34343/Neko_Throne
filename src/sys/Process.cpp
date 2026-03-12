#include "include/sys/Process.hpp"
#include "include/global/Configs.hpp"

#include <QTimer>
#include <QDir>
#include <QApplication>

#include "include/ui/mainwindow.h"

namespace Configs_sys {
    CoreProcess::~CoreProcess() {
        if (state() == QProcess::Running) {
            m_state = CoreLifecycleState::Stopping;
            terminate();
            if (!waitForFinished(3000)) {
                kill();
                waitForFinished(1000);
            }
        }
    }

    void CoreProcess::Kill() {
        if (state() != QProcess::Running) return;
        m_state = CoreLifecycleState::Stopping;
        terminate();
        if (!waitForFinished(1500)) {
            kill();
            waitForFinished(500);
        }
    }

    CoreProcess::CoreProcess(const QString &core_path, const QStringList &args) {
        program = core_path;
        arguments = args;

        connect(this, &QProcess::readyReadStandardOutput, this, [this]() {
            auto log = readAllStandardOutput();
            if (m_state == CoreLifecycleState::Starting) {
                if (log.contains("Core listening at")) {
                    // The core really started
                    m_state = CoreLifecycleState::Running;
                    Configs::dataStore->core_running = true;
                    MW_dialog_message("ExternalProcess", "CoreStarted," + Int2String(start_profile_when_core_is_up));
                    start_profile_when_core_is_up = -1;
                } else if (log.contains("failed to serve")) {
                    // The core failed to start
                    m_state = CoreLifecycleState::Failed;
                    kill();
                }
            }
            if (log.contains("Extra process exited unexpectedly"))
            {
                MW_show_log("Extra Core exited, stopping profile...");
                MW_dialog_message("ExternalProcess", "Crashed");
            }
            if (logCounter.fetchAndAddRelaxed(log.count("\n")) > Configs::dataStore->max_log_line) return;
            MW_show_log(log);
        });
        connect(this, &QProcess::readyReadStandardError, this, [this]() {
            auto log = readAllStandardError().trimmed();
            MW_show_log(log);
        });
        connect(this, &QProcess::errorOccurred, this, [this](ProcessError error) {
            if (error == FailedToStart) {
                failed_to_start = true;
                m_state = CoreLifecycleState::Failed;
                MW_show_log("start core error occurred: " + errorString() + "\n");
            }
        });
        connect(this, &QProcess::stateChanged, this, [this](ProcessState state) {
            if (state == NotRunning) {
                Configs::dataStore->core_running = false;
                if (m_state != CoreLifecycleState::Restarting
                    && m_state != CoreLifecycleState::Stopping) {
                    m_state = CoreLifecycleState::Stopped;
                }
                qDebug() << "Core state changed to not running";
            }

            if (!Configs::dataStore->prepare_exit && state == NotRunning) {
                if (failed_to_start) return; // no retry
                if (restarting) return;

                MW_show_log("[Fatal] " + QObject::tr("Core exited, cleaning up..."));
                runOnUiThread([=, this]
                {
                    GetMainWindow()->profile_stop(true, true);
                }, true);

                // Retry rate limit
                if (coreRestartTimer.isValid()) {
                    if (coreRestartTimer.restart() < 10 * 1000) {
                        coreRestartTimer = QElapsedTimer();
                        MW_show_log("[ERROR] " + QObject::tr("Core exits too frequently, stop automatic restart this profile."));
                        return;
                    }
                } else {
                    coreRestartTimer.start();
                }

                // Restart
                start_profile_when_core_is_up = Configs::dataStore->started_id;
                MW_show_log("[Warn] " + QObject::tr("Restarting the core ..."));
                setTimeout([=,this] { Restart(); }, this, 200);
            }
        });
    }

    void CoreProcess::Start() {
        if (started) return;
        started = true;
        m_state = CoreLifecycleState::Starting;

        // Reset log line counter so each fresh core session starts with a clean slate.
        // Without this reset, logs become permanently suppressed once the counter
        // exceeds max_log_line across the lifetime of the process object.
        logCounter.storeRelaxed(0);

        setEnvironment(QProcessEnvironment::systemEnvironment().toStringList());
        start(program, arguments);
    }

    void CoreProcess::Restart() {
        restarting = true;
        m_state = CoreLifecycleState::Restarting;
        kill();
        // Wait for the old process to exit; force-kill if it takes too long
        if (!waitForFinished(1500)) {
            kill();
            waitForFinished(500);
        }
        started = false;
        Start();
        // Delay clearing the restarting flag so that any pending stateChanged
        // signals from the dying process are delivered while restarting == true,
        // preventing a spurious crash-restart cycle.
        QTimer::singleShot(200, this, [this] { restarting = false; });
    }

} // namespace Configs_sys
