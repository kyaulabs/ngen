#include "screencast-client.h"
#include <QCommandLineParser>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSocketNotifier>
#include <QTimer>
#include <QUuid>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <pipewirerecord.h>
#include <unistd.h>
#include <wayland-client.h>

namespace {
qint64 now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
bool owned_process(int pid) {
    if (QFileInfo(QFileInfo("/proc/" + QString::number(pid) + "/exe").symLinkTarget()).fileName() !=
        "flight_demo")
        return false;
    for (unsigned depth = 0; pid > 1 && depth < 16; ++depth) {
        if (pid == getppid())
            return true;
        QFile status("/proc/" + QString::number(pid) + "/status");
        if (!status.open(QIODevice::ReadOnly))
            return false;
        const auto bytes = status.read(65537);
        if (bytes.size() > 65536)
            return false;
        bool found = false;
        for (const auto &line : QString::fromLatin1(bytes).split('\n')) {
            if (line.startsWith("PPid:")) {
                bool ok{};
                pid = line.mid(5).trimmed().toInt(&ok);
                if (!ok)
                    return false;
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return false;
}
} // namespace
class Capture final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kyaulabs.NgenCapture")
    int pid_, seconds_, script_id_{-1};
    bool until_close_;
    QString output_, status_, plugin_, service_, kwin_owner_, stop_reason_;
    QJsonObject window_;
    wl_display *display_{};
    wl_registry *registry_{};
    zkde_screencast_unstable_v1 *cast_{};
    zkde_screencast_stream_unstable_v1 *stream_{};
    std::unique_ptr<QSocketNotifier> notifier_;
    std::unique_ptr<PipeWireRecord> recorder_;
    bool stopping_{}, failed_{}, destroying_{};
    qint64 began_{}, ended_{};

    void report(const QString &state, const QString &error = {}) {
        QJsonObject data{{"schema", 1},
                         {"state", state},
                         {"window", window_},
                         {"target_pid", pid_},
                         {"plugin", plugin_},
                         {"recording_started_ns", began_},
                         {"recording_stopped_ns", ended_},
                         {"stop_reason", stop_reason_},
                         {"timestamp_ns", now_ns()},
                         {"system_audio", recorder_->recordSystemAudio()},
                         {"microphone", recorder_->recordMicrophone()},
                         {"error", error}};
        QSaveFile file(status_);
        const auto bytes = QJsonDocument(data).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
            !file.commit()) {
            failed_ = true;
            QCoreApplication::exit(1);
        }
    }
    void fail(const QString &error) {
        if (destroying_)
            return;
        failed_ = true;
        report("failed", error.left(1024));
        QCoreApplication::exit(1);
    }
    void complete() {
        report(failed_ ? "failed" : "complete");
        QCoreApplication::exit(failed_ ? 1 : 0);
    }
    void stop(const QString &reason) {
        if (stopping_)
            return;
        stopping_ = true;
        stop_reason_ = reason;
        ended_ = now_ns();
        if (recorder_->state() == PipeWireBaseEncodedStream::Idle) {
            complete();
            return;
        }
        recorder_->stop();
        QTimer::singleShot(15000, this, [this] { fail("encoder shutdown deadline"); });
    }

  public:
    struct Options {
        int pid{}, seconds{};
        bool until_close{};
        QString output, status, plugin;
    };
    explicit Capture(Options options)
        : pid_(options.pid), seconds_(options.seconds), until_close_(options.until_close),
          output_(std::move(options.output)), status_(std::move(options.status)),
          plugin_(std::move(options.plugin)),
          service_("org.kyaulabs.NgenCapture.p" + QString::number(getpid())),
          recorder_(std::make_unique<PipeWireRecord>()) {}
    ~Capture() override {
        destroying_ = true;
        notifier_.reset();
        recorder_->stop();
        recorder_.reset();
        if (stream_)
            zkde_screencast_stream_unstable_v1_close(stream_);
        if (cast_)
            zkde_screencast_unstable_v1_destroy(cast_);
        if (registry_)
            wl_registry_destroy(registry_);
        if (display_) {
            wl_display_flush(display_);
            wl_display_disconnect(display_);
        }
        if (script_id_ >= 0) {
            QDBusInterface scripting("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting");
            scripting.call("unloadScript", plugin_);
        }
    }
    bool start() {
        if (!owned_process(pid_))
            return false;
        auto bus = QDBusConnection::sessionBus();
        const QDBusReply<QString> owner = bus.interface()->serviceOwner("org.kde.KWin");
        if (!owner.isValid() || owner.value().isEmpty())
            return false;
        kwin_owner_ = owner.value();
        if (!bus.registerService(service_) ||
            !bus.registerObject("/Capture", this, QDBusConnection::ExportAllSlots))
            return false;
        display_ = wl_display_connect(nullptr);
        if (!display_)
            return false;
        registry_ = wl_display_get_registry(display_);
        static const wl_registry_listener registry_listener{
            [](void *data, wl_registry *registry, std::uint32_t name, const char *interface,
               std::uint32_t version) {
                auto &self = *static_cast<Capture *>(data);
                if (!std::strcmp(interface, "zkde_screencast_unstable_v1") && version >= 6)
                    self.cast_ = static_cast<zkde_screencast_unstable_v1 *>(wl_registry_bind(
                        registry, name, &zkde_screencast_unstable_v1_interface, 6));
            },
            [](void *, wl_registry *, std::uint32_t) {}};
        wl_registry_add_listener(registry_, &registry_listener, this);
        if (wl_display_roundtrip(display_) < 0 || !cast_)
            return false; // No monitor, KMS, X11 or other-window fallback.
        recorder_->setRecordSystemAudio(false);
        recorder_->setRecordMicrophone(false);
        if (recorder_->recordSystemAudio() || recorder_->recordMicrophone())
            return false;
        recorder_->setEncoder(PipeWireBaseEncodedStream::VP8);
        recorder_->setEncodingPreference(PipeWireBaseEncodedStream::Speed);
        // Use KPipeWire's documented bounded 50-frame analysis queue. A
        // three-frame queue saturated the filter before any encoded output.
        recorder_->setMaxPendingFrames(50);
        recorder_->setMaxFramerate(30);
        recorder_->setRequestedSize({1280, 720});
        recorder_->setOutput(output_);
        connect(recorder_.get(), &PipeWireRecord::errorFound, this,
                [this](const QString &error) { fail(error); });
        connect(recorder_.get(), &PipeWireRecord::stateChanged, this, [this] {
            if (destroying_)
                return;
            if (recorder_->state() == PipeWireBaseEncodedStream::Recording && !began_) {
                began_ = now_ns();
                report("recording");
                QTimer::singleShot(seconds_ * 1000, Qt::PreciseTimer, this,
                                   [this] { stop("time_limit"); });
            } else if (stopping_ && recorder_->state() == PipeWireBaseEncodedStream::Idle) {
                complete();
            }
        });
        notifier_ =
            std::make_unique<QSocketNotifier>(wl_display_get_fd(display_), QSocketNotifier::Read);
        connect(notifier_.get(), &QSocketNotifier::activated, this, [this] {
            if (wl_display_dispatch(display_) < 0)
                fail("Wayland capture disconnected");
        });
        const auto script_path = QFileInfo(status_).absolutePath() + "/select-owned-window.js";
        QFile script(script_path);
        if (!script.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            return false;
        const auto code = QStringLiteral(R"JS(
let sent = false;
function reportOwned() {
    if (sent) return;
    const owned = workspace.windowList().filter(w => w.pid === %1);
    if (owned.length !== 1) return;
    const w = owned[0];
    if (w.width < 2 || w.height < 2) {
        w.frameGeometryChanged.connect(reportOwned);
        return;
    }
    sent = true;
    workspace.activeWindow = w;
    callDBus('%2', '/Capture', 'org.kyaulabs.NgenCapture', 'window',
        JSON.stringify({pid: w.pid, uuid: w.internalId.toString(), width: w.width, height: w.height}));
}
workspace.windowAdded.connect(reportOwned);
reportOwned();
)JS")
                              .arg(pid_)
                              .arg(service_);
        if (script.write(code.toUtf8()) < 0)
            return false;
        script.close();
        QDBusInterface scripting("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting");
        const QDBusReply<int> loaded = scripting.call("loadScript", script_path, plugin_);
        if (!loaded.isValid() || loaded.value() < 0)
            return false;
        script_id_ = loaded.value();
        QDBusInterface owned_script("org.kde.KWin",
                                    "/Scripting/Script" + QString::number(script_id_),
                                    "org.kde.kwin.Script");
        owned_script.asyncCall("run");
        report("waiting_for_owned_window");
        QTimer::singleShot(20000, this, [this] {
            if (!began_)
                fail("owned window/capture startup deadline");
        });
        return true;
    }
  public Q_SLOTS:
    void window(const QString &payload) {
        if (stream_ || !calledFromDBus() || message().service() != kwin_owner_ ||
            payload.size() > 4096)
            return;
        const auto data = QJsonDocument::fromJson(payload.toUtf8()).object();
        const auto id = QUuid(data.value("uuid").toString());
        const auto width = data.value("width").toInt(), height = data.value("height").toInt();
        if (data.value("pid").toInt() != pid_ || id.isNull() || width < 2 || height < 2 ||
            width > 4096 || height > 4096) {
            fail("invalid owned window identity");
            return;
        }
        window_ = data;
        window_["uuid"] = id.toString(QUuid::WithoutBraces);
        const auto identifier = id.toString(QUuid::WithoutBraces).toUtf8();
        stream_ = zkde_screencast_unstable_v1_stream_window(
            cast_, identifier.constData(), ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_HIDDEN);
        static const zkde_screencast_stream_unstable_v1_listener listener{
            [](void *data, zkde_screencast_stream_unstable_v1 *) {
                auto &self = *static_cast<Capture *>(data);
                if (self.until_close_ && self.began_)
                    self.stop("window_closed");
                else
                    self.fail("owned window stream closed");
            },
            [](void *, zkde_screencast_stream_unstable_v1 *, std::uint32_t) {},
            [](void *data, zkde_screencast_stream_unstable_v1 *, const char *error) {
                static_cast<Capture *>(data)->fail(QString::fromUtf8(error));
            },
            [](void *data, zkde_screencast_stream_unstable_v1 *, std::uint32_t serialH,
               std::uint32_t serialL) {
                auto &self = *static_cast<Capture *>(data);
                const auto serial = (std::uint64_t{serialH} << 32U) | serialL;
                if (!serial) {
                    self.fail("invalid PipeWire object serial");
                    return;
                }
                self.recorder_->setObjectSerial(serial);
                self.recorder_->start();
            }};
        zkde_screencast_stream_unstable_v1_add_listener(stream_, &listener, this);
        wl_display_flush(display_);
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({{"pid", "Owned NGEN process", "pid"},
                       {"seconds", "Bounded recording seconds", "seconds"},
                       {"until-close", "Complete when the owned window closes"},
                       {"output", "Fresh video path", "output"},
                       {"status", "Private status path", "status"},
                       {"script-name", "Owned temporary KWin script identifier", "name"}});
    parser.process(app);
    bool pid_ok{}, seconds_ok{};
    const auto pid = parser.value("pid").toInt(&pid_ok);
    const auto seconds = parser.value("seconds").toInt(&seconds_ok);
    const auto output = parser.value("output"), status = parser.value("status");
    const bool until_close = parser.isSet("until-close");
    if (!pid_ok || pid <= 0 || !seconds_ok || seconds < 1 || seconds > (until_close ? 1800 : 90) ||
        output.isEmpty() || status.isEmpty() || QFileInfo::exists(output) ||
        QFileInfo::exists(status) || parser.value("script-name").isEmpty())
        return 2;
    Capture capture({.pid = pid,
                     .seconds = seconds,
                     .until_close = until_close,
                     .output = output,
                     .status = status,
                     .plugin = parser.value("script-name")});
    if (!capture.start())
        return 3;
    return app.exec();
}
#include "main.moc"
