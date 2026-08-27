#include "util/uid.hpp"

#include <array>
#include <mutex>
#include <string>

#include <switch.h>

#include "identity/identity_core.hpp"

namespace inst::util {
    std::string ComputeUidFromMmcCid()
    {
        static std::once_flag once;
        static std::string uid(64, '0');
        std::call_once(once, []() {
            FsDeviceOperator d = {};
            if (R_FAILED(fsOpenDeviceOperator(&d)))
                return;

            inst::identity::PersonaSeed mmcCid{};
            const Result rc = fsDeviceOperatorGetMmcCid(&d, mmcCid.data(), mmcCid.size(), static_cast<s64>(mmcCid.size()));
            fsDeviceOperatorClose(&d);
            if (R_FAILED(rc)) {
                std::fill(mmcCid.begin(), mmcCid.end(), 0);
                return;
            }

            std::string out = inst::identity::ComputeUidFromIdentityBytes(mmcCid);
            std::fill(mmcCid.begin(), mmcCid.end(), 0);
            uid = out;
        });
        return uid;
    }
}
