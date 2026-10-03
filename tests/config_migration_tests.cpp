#include "util/config.hpp"
#include <cassert>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    assert(argc == 2);
    std::filesystem::current_path(argv[1]);
    std::filesystem::create_directories(inst::config::remotesDir);
    std::ofstream(inst::config::configPath) << R"({"remoteUrl":"https://private.example","remoteUser":"fixture","remotePass":"fixture","remoteLegacyMode":true})";
    std::ofstream(inst::config::remotesDir + "/private.json") << R"({"remote":{"protocol":"https","host":"private.example","path":"/","port":443,"username":"fixture","password":"fixture","title":"My custom title","favourite":true,"legacyMode":true}})";
    inst::config::parseConfig();
    assert(inst::config::remoteCompatibility == inst::remote::Compatibility::Tinfoil);
    auto remotes = inst::config::LoadRemotes();
    auto found = std::find_if(remotes.begin(), remotes.end(), [](const auto& value) { return value.fileName == "private.json"; });
    assert(found != remotes.end());
    assert(found->title == "My custom title" && found->favourite && found->username == "fixture" && found->password == "fixture");
    assert(found->compatibility == inst::remote::Compatibility::Tinfoil);
    auto profile = *found;
    profile.compatibility = inst::remote::Compatibility::Auto;
    assert(inst::config::SaveRemote(profile));
    remotes = inst::config::LoadRemotes();
    found = std::find_if(remotes.begin(), remotes.end(), [](const auto& value) { return value.fileName == "private.json"; });
    assert(found != remotes.end() && found->compatibility == inst::remote::Compatibility::Auto);
    assert(found->title == profile.title && found->password == profile.password && found->favourite);
    assert(inst::config::SetActiveRemote(*found));
    inst::config::parseConfig();
    assert(inst::config::remoteCompatibility == inst::remote::Compatibility::Auto);
    std::cout << "Saved Remote/config migration and credential/title/favorite preservation tests passed\n";
}
