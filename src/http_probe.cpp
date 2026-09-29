#include "fol/core.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace fol {
QJsonObject analyzeHttpProbe(const QByteArray &report, const QString &session) {
    QJsonObject result{{"complete", false},
                       {"candidate_observed", false},
                       {"scope", "read-only candidate discovery; not routing or login"}};
    try {
        require(!report.isEmpty() && report.size() <= 65536 && report.endsWith('\n'),
                "HTTP probe report missing, truncated or oversized");
        bool started = false, child = false, candidate = false, complete = false;
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
            } else if (event == "completed") {
                require(candidate && record["value"].toInt(-1) == (discovered ? 0 : 67),
                        "Invalid HTTP probe completion record");
                complete = true;
                result["reported_helper_exit_code"] = record["value"];
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
} // namespace fol
