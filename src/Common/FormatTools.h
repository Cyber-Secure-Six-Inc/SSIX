// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) | 2020-2021 Cyber Secure Six Inc. | 2016 - 2019 The Karbo Developers
// 
// Please see the included LICENSE file for more information.

#pragma once 

#include <string>
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

namespace Common
{
    std::string get_mining_speed(const uint64_t hashrate);

    std::string get_sync_percentage(
        uint64_t height,
        const uint64_t target_height);

    std::string formatAmountWithTicker(const uint64_t amount);
	
	std::string formatAmount(uint64_t amount);

	std::string formatAmount(int64_t amount);

    std::string formatAmountBasic(const uint64_t amount);

    bool parseAmount(const std::string& str, uint64_t& amount);

    std::string prettyPrintBytes(const uint64_t numBytes);

    std::string unixTimeToDate(const uint64_t timestamp);
}
