// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) | 2020-2021 Cyber Secure Six Inc. | 2016 - 2019 The Karbo Developers
//
// This file is part of SSIX.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#include "DBUtils.h"

namespace {
  const std::string RAW_BLOCK_NAME = "raw_block";
  const std::string RAW_TXS_NAME = "raw_txs";
}

namespace CryptoNote {
namespace DB {
  std::string serialize(const RawBlock& value, const std::string& name) {
    std::stringstream ss;
    Common::StdOutputStream stream(ss);
    CryptoNote::BinaryOutputStreamSerializer serializer(stream);
    
    serializer(const_cast<RawBlock&>(value).block, RAW_BLOCK_NAME);
    serializer(const_cast<RawBlock&>(value).transactions, RAW_TXS_NAME);

    return ss.str();
  }

  void deserialize(const std::string& serialized, RawBlock& value, const std::string& name) {
    std::stringstream ss(serialized);
    Common::StdInputStream stream(ss);
    CryptoNote::BinaryInputStreamSerializer serializer(stream);
    serializer(value.block, RAW_BLOCK_NAME);
    serializer(value.transactions, RAW_TXS_NAME);
  }
}
}
