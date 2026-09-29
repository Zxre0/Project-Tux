#include "fol/core.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace fol {
QJsonObject analyzeHttpProbe(const QByteArray &report, const QString &session) {
    QJsonObject result{{"complete", false},
                       {"candidate_observed", false},
                       {"scope", "diagnostic interface evidence; not routing or login"}};
    try {
        require(!report.isEmpty() && report.size() <= 65536 && report.endsWith('\n'),
                "HTTP probe report missing, truncated or oversized");
        bool started = false, child = false, candidate = false, complete = false;
        bool attached = false, observationComplete = false;
        int requests = 0;
        int control = 0;
        bool controlObserved = false;
        int discovered = -1;
        for (const auto &line : report.split('\n')) {
            if (line.isEmpty())
                continue;
            require(!complete && line.startsWith("FOL_HTTP_PROBE "),
                    "Unexpected HTTP probe report content");
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(line.mid(15), &error);
            require(error.error == QJsonParseError::NoError && doc.isObject(),
                    "Malformed HTTP probe record");
            const auto record = doc.object();
            const auto event = record["event"].toString();
            if (!started) {
                require(event == "started" && record["session_id"].toString() == session &&
                            !session.isEmpty(),
                        "HTTP probe session mismatch");
                started = true;
            } else if (event == "owned_child_started") {
                require(!child && record["value"].toInt() > 0, "Invalid HTTP probe child record");
                child = true;
                result["child_windows_pid"] = record["value"];
            } else if (event == "interface_candidate_observed") {
                discovered = record["value"].toInt(-1);
                require(child && !candidate && (discovered == 0 || discovered == 1),
                        "Invalid HTTP probe candidate record");
                candidate = true;
                result["candidate_observed"] = discovered == 1;
            } else if (event == "completed") {
                require(candidate && (!attached || observationComplete) &&
                            (control != 1 || controlObserved) &&
                            record["value"].toInt(-1) == (discovered && !control ? 0 : 67),
                        "Invalid HTTP probe completion record");
                complete = true;
                result["reported_helper_exit_code"] = record["value"];
            } else if (event == "debug_attached") {
                require(candidate && discovered == 1 && !attached && record["value"].toInt() > 0,
                        "Invalid request-observer attachment record");
                attached = true;
            } else if (event == "module_loaded" || event == "module_control_child_live") {
                require(candidate && discovered == 1 && !attached && record["value"].toInt(-1) == 1,
                        "Module load or owned-child liveness check failed");
                result[event] = true;
            } else if (event == "observer_delay_ms") {
                require(candidate && !attached && record["value"].toInt(-1) >= 0 &&
                            record["value"].toInt(-1) <= 30000,
                        "Invalid observer delay record");
                result["observer_delay_ms"] = record["value"];
            } else if (event == "observer_control") {
                control = record["value"].toInt();
                require(candidate && !attached && (control == 1 || control == 2),
                        "Unexpected observer control record");
                result[event] = control;
            } else if (event == "observer_control_child_live") {
                require(control == 1 && !attached && !controlObserved &&
                            (record["value"].toInt(-1) == 0 || record["value"].toInt(-1) == 1),
                        "Invalid observer control child state");
                controlObserved = true;
                result[event] = record["value"];
            } else if (event == "request_observed") {
                const QStringList origins{"epic", "loopback", "other"};
                const QStringList routes{"oauth-token",      "account-public", "fortnite-profile",
                                         "fortnite-version", "lightswitch",    "launcher",
                                         "content",          "other"};
                require(attached && !observationComplete && requests < 8 &&
                            origins.contains(record["origin"].toString()) &&
                            routes.contains(record["route"].toString()),
                        "Invalid or non-redacted request observation");
                ++requests;
                auto observations = result["requests"].toArray();
                observations.append(
                    QJsonObject{{"origin", record["origin"]}, {"route", record["route"]}});
                result["requests"] = observations;
            } else if (event == "request_observation_completed") {
                require(attached && !observationComplete &&
                            (control == 2 ? requests == 0 && record["value"].toInt(-1) == 0
                                          : requests > 0 && record["value"].toInt(-1) == 1),
                        "Request observer did not complete a valid observation");
                observationComplete = true;
                result["request_observation_completed"] = control != 2;
            } else if (event == "debug_child_exited") {
                require(attached && !observationComplete, "Unexpected observed child exit");
                result["observed_child_exit_code"] = record["value"];
            } else if (event == "pointer_table") {
                const auto functions = record["function_rvas"].toArray();
                const auto slot = record["candidate_slot"].toInt(-1);
                require(child && !candidate && !functions.isEmpty() && functions.size() <= 129 &&
                            slot >= 0 && slot < functions.size() &&
                            record["start_rva"].toInteger(-1) > 0 &&
                            record["reference_rva"].toInteger(-1) ==
                                record["start_rva"].toInteger() + slot * 8 &&
                            record["scan_limit_reached"].isBool(),
                        "Invalid HTTP probe pointer-table record");
                auto tables = result["pointer_tables"].toArray();
                tables.append(record);
                result["pointer_tables"] = tables;
            } else if (event == "code_observation") {
                static const QRegularExpression hex("\\A[0-9a-f]{256}\\z");
                require(child && !candidate && record["function_rva"].toInteger() > 0 &&
                            record["table_slot"].toInt(-1) >= 0 &&
                            record["table_slot"].toInt(-1) < 129 &&
                            hex.match(record["bytes"].toString()).hasMatch(),
                        "Invalid bounded code observation");
                // Keep executable-byte samples only in the explicit local probe
                // file; do not propagate them into the general launch report.
                result["code_observations"] = result["code_observations"].toInt() + 1;
            } else {
                require(child && !candidate &&
                            (event == "marker_matches" || event == "xref_matches_up_to_limit" ||
                             event == "candidate"),
                        "HTTP probe failed or emitted an unexpected record");
            }
        }
        require(complete, "HTTP probe did not complete; wrapper exit is not helper success");
        result["complete"] = true;
        result["candidate_observed"] = discovered == 1;
    } catch (const std::exception &error) {
        result["error"] = QString::fromUtf8(error.what());
    }
    return result;
}
QJsonObject analyzeModuleReceipt(const QByteArray &report, const QString &session,
                                 qint64 childPid) {
    QJsonObject result{{"ready", false}, {"scope", "module lifecycle only; not routing or login"}};
    try {
        require(!report.isEmpty() && report.size() <= 4096 && report.endsWith('\n'),
                "Module receipt missing, oversized or truncated");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(report, &error);
        require(error.error == QJsonParseError::NoError && document.isObject(),
                "Invalid module receipt JSON");
        const auto object = document.object();
        require(object["event"] == "module-ready" && !session.isEmpty() &&
                    object["session_id"].toString() == session && childPid > 0 &&
                    object["process_id"].toInteger() == childPid,
                "Module receipt event/session/child identity mismatch");
        result["ready"] = true;
        result["process_id"] = childPid;
    } catch (const std::exception &error) {
        result["error"] = QString::fromUtf8(error.what());
    }
    return result;
}
QJsonObject analyzeModuleObservation(const QByteArray &report, const QString &session,
                                     qint64 childPid) {
    QJsonObject result{{"observed", false},
                       {"scope", "request categories only; not routing or login"}};
    try {
        require(!report.isEmpty() && report.size() <= 4096 && report.endsWith('\n'),
                "Module observation missing, oversized or truncated");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(report, &error);
        require(error.error == QJsonParseError::NoError && document.isObject(),
                "Invalid module observation JSON");
        const auto object = document.object();
        require(object["event"] == "module-observation" && !session.isEmpty() &&
                    object["session_id"].toString() == session && childPid > 0 &&
                    object["process_id"].toInteger() == childPid,
                "Module observation event/session/child identity mismatch");
        const QStringList errors{"setter-rejected",
                                 "image-rejected",
                                 "section-rejected",
                                 "pointer-rejected",
                                 "page-rejected",
                                 "protect-failed",
                                 "none",
                                 "layout-rejected",
                                 "pointer-changed",
                                 "restore-failed",
                                 "protection-failed",
                                 "request-shape-rejected",
                                 "no-requests"};
        require(errors.contains(object["error"].toString()) && object["installed"].isBool() &&
                    object["restored"].isBool() && object["requests"].isArray() &&
                    object["rejected"].toInt(-1) >= 0 && object["rejected"].toInt() <= 8,
                "Malformed module observation result");
        result["installed"] = object["installed"];
        result["restored"] = object["restored"];
        result["rejected"] = object["rejected"];
        if (object.contains("routed")) {
            require(object["routed"].toInt(-1) >= 0 && object["routed"].toInt() <= 8,
                    "Invalid local routing count");
            result["routed"] = object["routed"];
            result["scope"] = "local URL rewrite evidence; not transport or authentication";
        }
        result["reported_error"] = object["error"];
        const auto requests = object["requests"].toArray();
        require(requests.size() + object["rejected"].toInt() <= 8, "Too many request observations");
        const QStringList origins{"epic", "loopback", "other"};
        const QStringList routes{"oauth-token",      "account-public", "fortnite-profile",
                                 "fortnite-version", "lightswitch",    "launcher",
                                 "content",          "other"};
        QJsonArray checked;
        for (const auto &entry : requests) {
            const auto request = entry.toObject();
            require(entry.isObject() && request.size() == 2 &&
                        origins.contains(request["origin"].toString()) &&
                        routes.contains(request["route"].toString()),
                    "Non-redacted or invalid request category");
            checked.append(request);
        }
        result["requests"] = checked;
        require(object["installed"].toBool() && object["restored"].toBool() &&
                    object["rejected"].toInt() == 0 && object["error"] == "none" &&
                    !checked.isEmpty(),
                "Module did not complete clean request observation and pointer restoration");
        result["observed"] = true;
    } catch (const std::exception &error) {
        result["error"] = QString::fromUtf8(error.what());
    }
    return result;
}
} // namespace fol
