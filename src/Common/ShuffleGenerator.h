// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) | 2020-2021 Cyber Secure Six Inc. | 2016 - 2019 The Karbo Developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
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

#pragma once

#include <crypto/random.h>

#include <unordered_map>

class SequenceEnded: public std::runtime_error {
public:
  SequenceEnded() : std::runtime_error("shuffle sequence ended") {
  }

  ~SequenceEnded(){}
};

template <typename T>
class ShuffleGenerator {
public:

  ShuffleGenerator(T n) :
    N(n), count(n) {}

  T operator()() {

    if (count == 0) {
      throw SequenceEnded();
    }

    T value = Random::randomValue<T>(0, --count);

    auto rvalIt = selected.find(count);
    auto rval = rvalIt != selected.end() ? rvalIt->second : count;

    auto lvalIt = selected.find(value);

    if (lvalIt != selected.end()) {
      value = lvalIt->second;
      lvalIt->second = rval;
    } else {
      selected[value] = rval;
    }

    return value;
  }

  bool empty() const {
    return count == 0;
  }

  void reset() {
    count = N;
    selected.clear();
  }

private:

  std::unordered_map<T, T> selected;
  T count;
  const T N;
};
