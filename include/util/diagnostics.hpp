#pragma once

#include <string>

namespace inst::diagnostics {
    bool ExportDiagnosticReport(std::string* outPath = nullptr, std::string* error = nullptr);
}
