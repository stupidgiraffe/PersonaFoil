#include "util/bridge_core.hpp"

#include <cassert>
#include <iostream>

using namespace inst::bridge;

int main() {
    std::string error;
    Catalog catalog;
    const std::string validCatalog = R"({
        "schema_version":1,
        "generated_at":"2026-10-04T00:00:00Z",
        "sources":[
            {"id":"telegram-main","name":"Telegram","kind":"telegram","health":"online","priority":10},
            {"id":"dbi-main","name":"DBI HTTP","kind":"dbi_http","health":"degraded","priority":20}
        ],
        "releases":[
            {
                "id":"rel-update-tg",
                "title_id":"0100abcdef120800",
                "content_type":"update",
                "version":131072,
                "size":1000,
                "display_name":"Example Update",
                "source_id":"telegram-main",
                "download_id":"tg:42:99",
                "sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                "region":"US",
                "languages":["en","ja"]
            },
            {
                "id":"rel-update-dbi",
                "title_id":"0100ABCDEF120800",
                "content_type":"update",
                "version":131072,
                "size":1000,
                "source_id":"dbi-main",
                "download_id":"dbi:item-99"
            }
        ]
    })";
    assert(ParseCatalog(validCatalog, catalog, error));
    assert(error.empty());
    assert(catalog.sources.size() == 2);
    assert(catalog.releases.size() == 2);
    assert(catalog.releases[0].titleId == "0100ABCDEF120800");
    assert(catalog.releases[0].contentType == ContentType::Update);
    assert(catalog.releases[0].languages.size() == 2);

    Catalog badCatalog;
    assert(!ParseCatalog(R"({"schema_version":2,"sources":[],"releases":[]})", badCatalog, error));
    assert(!error.empty());
    assert(!ParseCatalog(R"({"schema_version":1,"sources":[],"releases":[
        {"id":"x","title_id":"0100ABCDEF120800","content_type":"update","version":1,"size":1,
         "source_id":"missing","download_id":"x"}
    ]})", badCatalog, error));
    assert(error == "Release references an unknown source.");

    ReconcileResult result;
    const std::string validReconcile = R"({
        "schema_version":1,
        "actions":[
            {
                "title_id":"0100abcdef120800",
                "content_type":"update",
                "installed_version":65536,
                "latest_version":131072,
                "reason":"update",
                "alternatives":[
                    {"release_id":"rel-update-tg","source_id":"telegram-main","download_id":"tg:42:99","version":131072,"size":1000},
                    {"release_id":"rel-update-dbi","source_id":"dbi-main","download_id":"dbi:item-99","version":131072,"size":1000}
                ]
            },
            {
                "title_id":"0100ABCDEF121001",
                "content_type":"dlc",
                "installed_version":0,
                "latest_version":0,
                "reason":"missing",
                "alternatives":[
                    {"release_id":"rel-dlc","source_id":"dbi-main","download_id":"dbi:dlc","version":0,"size":10}
                ]
            }
        ],
        "warnings":["Telegram source is temporarily delayed."]
    })";
    assert(ParseReconcile(validReconcile, result, error));
    assert(result.actions.size() == 2);
    assert(result.actions[0].alternatives.size() == 2);
    assert(result.actions[0].titleId == "0100ABCDEF120800");
    assert(result.actions[1].reason == ActionReason::Missing);
    assert(result.warnings.size() == 1);

    assert(!ParseReconcile(R"({
        "schema_version":1,
        "actions":[{
            "title_id":"0100ABCDEF120800","content_type":"update",
            "installed_version":2,"latest_version":2,"reason":"update",
            "alternatives":[{"release_id":"r","source_id":"s","download_id":"d","version":2,"size":1}]
        }]
    })", result, error));
    assert(error == "Update action is not newer than installed content.");

    std::vector<InstalledTitle> installed = {
        {"0100abcdef120000", ContentType::Base, 0},
        {"0100ABCDEF120800", ContentType::Update, 65536},
        {"0100ABCDEF121001", ContentType::Dlc, 0}
    };
    const auto request = BuildReconcileRequest(installed, error);
    assert(error.empty());
    assert(request["schema_version"] == 1);
    assert(request["installed"].size() == 3);
    assert(request["installed"][0]["title_id"] == "0100ABCDEF120000");
    assert(request["installed"][1]["content_type"] == "update");

    installed.push_back({"0100abcdef120800", ContentType::Update, 65536});
    const auto duplicate = BuildReconcileRequest(installed, error);
    assert(duplicate.empty());
    assert(error == "Duplicate installed title record.");

    std::cout << "PersonaBridge contract tests passed\n";
    return 0;
}
