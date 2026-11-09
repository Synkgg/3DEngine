#pragma once

#include <string>

namespace PlayerAccount
{
    std::string PublicKey();
    std::string Sign(const std::string& action, const std::string& nonce);
}
