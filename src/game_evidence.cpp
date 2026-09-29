#include "fol/core.h"
#include <QRegularExpression>

namespace fol {
QJsonObject analyzeGameLog(const QByteArray &data) {
    require(data.size() <= 64 * 1024 * 1024, "Game log exceeds 64 MiB analysis limit");
    // Match complete known diagnostic lines, not substrings in command lines,
    // chat or HTTP payloads. Only allowlisted fields enter the JSON report.
    static const QRegularExpression timestamp(R"(^\[[0-9.:-]+\]\[\s*[0-9]+\])");
    static const QRegularExpression engine(
        R"(^LogInit: (Compatible )?Engine Version: ([A-Za-z0-9.+_-]{1,160})$)");
    static const QRegularExpression backend(
        R"(^LogOnlineGame: Using EpicApp backend '([A-Za-z0-9._-]{1,64})' with host '([A-Za-z0-9.:-]{1,253}|<unconfigured>)' based on environment '([A-Za-z0-9._-]{1,64})' and override '([A-Za-z0-9._-]{0,64})'$)");
    static const QRegularExpression missing(
        R"(^LogOnlineGame: Error: Did not find Fortnite MCPConfig ([A-Za-z0-9._-]{1,64}) for service ([A-Za-z0-9._-]{1,64})$)");
    static const QRegularExpression loginAbort(
        R"(^LogOnlineAccount: Warning: \[UOnlineAccountCommon::AbortLoggingIn\] Aborting OnlineAccountCommon login due to error \[CurrentLoginStep=([A-Za-z0-9_]{1,64})\] \[LoginResult=([0-9]{1,5})\] \[DisplayError=.*\] \[DetailedError=.*\]$)");
    QJsonObject result{{"scope", "recognized game-log observations; not proof of rendering, "
                                 "authentication, gameplay or network reachability"},
                       {"engine_version", "unknown"},
                       {"compatible_engine_version", "unknown"}};
    QJsonArray evidence;
    int matches = 0, skipped = 0, lineNumber = 0;
    bool missingLogged = false, rejected = false, signIn = false, loginAborted = false;
    qsizetype offset = 0;
    while (offset < data.size()) {
        auto end = data.indexOf('\n', offset);
        if (end < 0)
            end = data.size();
        const auto length = end - offset;
        ++lineNumber;
        if (length > 4096) {
            ++skipped;
            offset = end + 1;
            continue;
        }
        auto line = QString::fromUtf8(data.constData() + offset, length);
        offset = end + 1;
        if (line.endsWith('\r'))
            line.chop(1);
        if (lineNumber == 1 && line.startsWith(QChar(0xfeff)))
            line.remove(0, 1);
        line.remove(timestamp);
        QJsonObject entry{{"line", lineNumber}};
        auto match = engine.match(line);
        if (match.hasMatch()) {
            const auto key =
                match.captured(1).isEmpty() ? "engine_version" : "compatible_engine_version";
            result[key] = match.captured(2);
            entry["kind"] = key;
            entry["value"] = match.captured(2);
        } else if ((match = backend.match(line)).hasMatch()) {
            QJsonObject selection{{"name", match.captured(1)},
                                  {"host", match.captured(2)},
                                  {"environment", match.captured(3)},
                                  {"override", match.captured(4)}};
            result["last_backend_selection"] = selection;
            entry["kind"] = "backend_selection";
            entry["value"] = selection;
        } else if ((match = missing.match(line)).hasMatch()) {
            missingLogged = true;
            entry["kind"] = "missing_backend_configuration";
            entry["config"] = match.captured(1);
            entry["service"] = match.captured(2);
        } else if (line == "LogOnlineGame: Error: Invalid backend requested! Exiting.") {
            rejected = true;
            entry["kind"] = "backend_rejected";
        } else if ((match = loginAbort.match(line)).hasMatch()) {
            loginAborted = true;
            // Error strings can contain arbitrary backend payloads or credentials.
            // Retain only the bounded stage and numeric result from this marker.
            QJsonObject failure{{"stage", match.captured(1)},
                                {"result", match.captured(2).toInt()}};
            result["last_logged_login_failure"] = failure;
            entry["kind"] = "login_aborted";
            entry["value"] = failure;
        } else if (line == "LogUIActionRouter: Display: Applying input config for leaf-most "
                           "node [SignIn_Credentials]" ||
                   line == "LogFortLogin: Focusing widget SignIn_Credentials") {
            signIn = true;
            entry["kind"] = "manual_sign_in";
        } else {
            continue;
        }
        ++matches;
        if (evidence.size() < 128)
            evidence.append(entry);
    }
    result["backend_configuration_missing_logged"] = missingLogged;
    result["backend_rejected_logged"] = rejected;
    result["manual_sign_in_logged"] = signIn;
    result["login_aborted_logged"] = loginAborted;
    result["interpretation"] = rejected       ? "backend rejection logged; inspect configuration"
                               : loginAborted ? "login abort logged; successful login unverified"
                               : signIn       ? "manual sign-in logged; login/gameplay unverified"
                                              : "no recognized terminal startup observation";
    result["evidence"] = evidence;
    result["matched_lines"] = matches;
    result["evidence_truncated"] = matches > evidence.size();
    result["oversized_lines_skipped"] = skipped;
    return result;
}
} // namespace fol
