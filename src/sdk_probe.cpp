#include "fol/core.h"
#include <QJsonDocument>

namespace fol {
QJsonObject analyzeSdkProbe(const QByteArray &data, const QString &session, qint64 childPid) {
    QJsonObject result{{"complete", false},
                       {"scope", "SDK version/reflection observations; not hosting or gameplay"}};
    try {
        require(!data.isEmpty() && data.size() <= 16384 && data.endsWith('\n') &&
                    !session.isEmpty() && childPid > 0,
                "SDK receipt missing, oversized, truncated or child identity unavailable");
        const QStringList sequence{"started",
                                   "sdk-init-started",
                                   "sdk-init-completed",
                                   "fortnite-version-times100",
                                   "engine-version-times100",
                                   "object-count",
                                   "game-mode-class-found",
                                   "net-driver-class-found",
                                   "ready-to-start-function-found",
                                   "process-event-vtable-slot",
                                   "world-found"};
        int stage = 0;
        bool finished = false, failure = false;
        QJsonObject observations;
        for (const auto &line : data.split('\n')) {
            if (line.isEmpty())
                continue;
            require(!finished, "Unexpected SDK record after completion");
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(line, &error);
            require(error.error == QJsonParseError::NoError && doc.isObject(),
                    "Invalid SDK record");
            const auto record = doc.object();
            const auto value = record["value"].toDouble(-1);
            require(record.size() == 4 && record["session_id"] == session &&
                        record["process_id"].toInteger(-1) == childPid && value >= 0 &&
                        value <= 4294967295.0 && value == static_cast<quint64>(value),
                    "SDK record schema, session, child or numeric value mismatch");
            const auto event = record["event"].toString();
            if (event == "completed") {
                finished = true;
                require(value == 1 && stage == sequence.size() && !failure,
                        "SDK probe reported unsatisfied prerequisites");
            } else if (event == "cpp-exception" || event == "unknown-cpp-exception" ||
                       event == "structured-exception") {
                require(stage > 0 && !failure, "Unexpected SDK failure record");
                failure = true;
                result["failure_event"] = event;
                result["failure_code"] = value;
            } else {
                require(!failure && stage < sequence.size() && sequence[stage] == event,
                        "SDK stage missing, duplicated or out of order");
                ++stage;
                observations[event] = value;
                result["observations"] = observations;
                result["last_completed_stage"] = event;
            }
        }
        require(finished, "SDK probe did not finish");
        require(observations["fortnite-version-times100"] == 1250 &&
                    observations["engine-version-times100"] == 425 &&
                    observations["object-count"].toInteger() > 0 &&
                    observations["object-count"].toInteger() <= 2000000 &&
                    observations["process-event-vtable-slot"].toInteger() > 0 &&
                    observations["process-event-vtable-slot"].toInteger() < 256,
                "SDK version, object count or reflection slot is not compatible");
        for (const auto &key : sequence)
            if (key.endsWith("-found") || key == "started" || key.startsWith("sdk-init-"))
                require(observations[key] == 1, "SDK required prerequisite not found");
        result["observations"] = observations;
        result["complete"] = true;
    } catch (const std::exception &error) {
        result["error"] = QString::fromUtf8(error.what());
    }
    return result;
}
} // namespace fol
