#include "../helpers/windows/local_url.h"
#include <cwchar>
#include <iostream>

int main() {
    unsigned checks = 0;
    wchar_t result[4096];
    const auto check = [&](bool condition) {
        ++checks;
        if (!condition)
            throw checks;
    };
    try {
        check(fol_local_url(L"https://account-public-service-prod.ol.epicgames.com/account/api/"
                            L"oauth/token?local-test=1",
                            result, 4096) == 1);
        check(std::wcscmp(result, L"http://127.0.0.1:3551/account/api/oauth/token?local-test=1") ==
              0);
        check(
            fol_local_url(L"https://fortnite-public-service.ol.epicgames.net/fortnite/api/version",
                          result, 4096) == 1);
        check(fol_local_url(
                  L"https://party-service-prod.ol.epicgames.com/party/api/v1/Fortnite/parties",
                  result, 4096) == 1);
        check(std::wcscmp(result, L"http://127.0.0.1:3551/party/api/v1/Fortnite/parties") == 0);
        for (const wchar_t *url :
             {L"https://attacker.invalid/account/api/oauth/token",
              L"https://account.ol.epicgames.com.attacker.invalid/account/api/oauth/token",
              L"https://account.notol.epicgames.com/account/api/oauth/token",
              L"https://account.ol.epicgames.com/telemetry/",
              L"https://account.ol.epicgames.com/account/apix/token",
              L"https://party-service-prod.ol.epicgames.com/party/api/v1/OtherGame/parties",
              L"https://party-service-prod.ol.epicgames.com/party/api/v1/Fortnitex/parties",
              L"http://127.0.0.1:3551/account/api/oauth/token"}) {
            check(fol_local_url(url, result, 4096) == 0);
            check(result[0] == 0);
        }
        for (const wchar_t *url :
             {L"https://user@account.ol.epicgames.com/account/api/oauth/token",
              L"https://account.ol.epicgames.com:443/account/api/oauth/token",
              L"https://account.ol.epicgames.com/account/api/oauth/token#fragment",
              L"https://account.ol.epicgames.com/account/api/\r\nInjected: header",
              L"https://account.ol.epicgames.com\\account/api/", L"https:///account/api/", L""})
            check(fol_local_url(url, result, 4096) == -1);
        check(fol_local_url(L"https://account.ol.epicgames.com/account/api/oauth/token", result,
                            5) == -1);
        check(fol_local_url(nullptr, result, 4096) == -1);
        check(fol_local_url(L"https://account.ol.epicgames.com/account/api/oauth/token", nullptr,
                            4096) == -1);
        std::wcout << checks << L" local URL selection checks passed\n";
        return 0;
    } catch (unsigned failed) {
        std::cerr << "Local URL check failed: " << failed << '\n';
        return 1;
    }
}
