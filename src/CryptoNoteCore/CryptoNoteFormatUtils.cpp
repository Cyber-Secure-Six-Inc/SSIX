// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2020, The Karbo developers
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

#include "CryptoNoteFormatUtils.h"

#include <set>
#include "Logging/LoggerRef.h"
#include "Common/Varint.h"
#include "Common/Base58.h"

#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "CryptoNoteSerialization.h"

#include "Account.h"
#include "CryptoNoteBasicImpl.h"
#include "CryptoNoteSerialization.h"
#include "TransactionExtra.h"
#include "CryptoNoteTools.h"
#include "Currency.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

#include "CryptoNoteConfig.h"

using namespace Logging;
using namespace Crypto;
using namespace Common;

namespace CryptoNote {

bool parseAndValidateTransactionFromBinaryArray(const BinaryArray& tx_blob, Transaction& tx, Hash& tx_hash, Hash& tx_prefix_hash) {
  if (!fromBinaryArray(tx, tx_blob)) {
    return false;
  }

  //TODO: validate tx
  cn_fast_hash(tx_blob.data(), tx_blob.size(), tx_hash);
  getObjectHash(*static_cast<TransactionPrefix*>(&tx), tx_prefix_hash);
  return true;
}

bool generate_key_image_helper(const AccountKeys& ack, const PublicKey& tx_public_key, size_t real_output_index, KeyPair& in_ephemeral, KeyImage& ki) {
  KeyDerivation recv_derivation;
  bool r = generate_key_derivation(tx_public_key, ack.viewSecretKey, recv_derivation);

  assert(r && "key image helper: failed to generate_key_derivation");

  if (!r) {
    return false;
  }

  r = derive_public_key(recv_derivation, real_output_index, ack.address.spendPublicKey, in_ephemeral.publicKey);

  assert(r && "key image helper: failed to derive_public_key");

  if (!r) {
    return false;
  }

  derive_secret_key(recv_derivation, real_output_index, ack.spendSecretKey, in_ephemeral.secretKey);
  generate_key_image(in_ephemeral.publicKey, in_ephemeral.secretKey, ki);
  return true;
}

uint64_t power_integral(uint64_t a, uint64_t b) {
  if (b == 0)
    return 1;
  uint64_t total = a;
  for (uint64_t i = 1; i != b; i++)
    total *= a;
  return total;
}

bool get_tx_fee(const Transaction& tx, uint64_t & fee) {
  uint64_t amount_in = 0;
  uint64_t amount_out = 0;

  for (const auto& in : tx.inputs) {
    if (in.type() == typeid(KeyInput)) {
      amount_in += boost::get<KeyInput>(in).amount;
    } else if (in.type() == typeid(MultisignatureInput)) {
      amount_in += boost::get<MultisignatureInput>(in).amount;
    }
  }

  for (const auto& o : tx.outputs) {
    amount_out += o.amount;
  }

  if (!(amount_in >= amount_out)) {
    return false;
  }

  fee = amount_in - amount_out;
  return true;
}

uint64_t get_tx_fee(const Transaction& tx) {
  uint64_t r = 0;
  if (!get_tx_fee(tx, r))
    return 0;
  return r;
}

std::vector<uint32_t> relativeOutputOffsetsToAbsolute(const std::vector<uint32_t>& off) {
  std::vector<uint32_t> res = off;
  for (size_t i = 1; i < res.size(); i++)
    res[i] += res[i - 1];
  return res;
}

std::vector<uint32_t> absolute_output_offsets_to_relative(const std::vector<uint32_t>& off) {
  if (off.empty()) return {};
  auto copy = off;
  for (size_t i = 1; i < copy.size(); ++i) {
    copy[i] = off[i] - off[i-1];
  }
  return copy;
}

bool generateDeterministicTransactionKeys(const Crypto::Hash& inputsHash, const Crypto::SecretKey& viewSecretKey, KeyPair& generatedKeys) {
  BinaryArray ba;
  append(ba, std::begin(viewSecretKey.data), std::end(viewSecretKey.data));
  append(ba, std::begin(inputsHash.data), std::end(inputsHash.data));

  hash_to_scalar(ba.data(), ba.size(), generatedKeys.secretKey);
  return Crypto::secret_key_to_public_key(generatedKeys.secretKey, generatedKeys.publicKey);
}

bool generateDeterministicTransactionKeys(const Transaction& tx, const SecretKey& viewSecretKey, KeyPair& generatedKeys) {
  Crypto::Hash inputsHash = getObjectHash(tx.inputs);
  return generateDeterministicTransactionKeys(inputsHash, viewSecretKey, generatedKeys);
}

bool constructTransaction(
  const AccountKeys& sender_account_keys,
  const std::vector<TransactionSourceEntry>& sources,
  const std::vector<TransactionDestinationEntry>& destinations,
  std::vector<uint8_t> extra,
  Transaction& tx,
  uint64_t unlock_time,
  Crypto::SecretKey &tx_key,
  Logging::ILogger& log) {
  LoggerRef logger(log, "construct_tx");

  tx.inputs.clear();
  tx.outputs.clear();
  tx.signatures.clear();

  tx.version = CURRENT_TRANSACTION_VERSION;
  tx.unlockTime = unlock_time;

  tx.extra = extra;
 
  struct input_generation_context_data {
    KeyPair in_ephemeral;
  };

  std::vector<input_generation_context_data> in_contexts;
  uint64_t summary_inputs_money = 0;
  //fill inputs
  for (const TransactionSourceEntry& src_entr : sources) {
    if (src_entr.realOutput >= src_entr.outputs.size()) {
      logger(ERROR) << "real_output index (" << src_entr.realOutput << ")bigger than output_keys.size()=" << src_entr.outputs.size();
      return false;
    }
    summary_inputs_money += src_entr.amount;

    //KeyDerivation recv_derivation;
    in_contexts.push_back(input_generation_context_data());
    KeyPair& in_ephemeral = in_contexts.back().in_ephemeral;
    KeyImage img;
    if (!generate_key_image_helper(sender_account_keys, src_entr.realTransactionPublicKey, src_entr.realOutputIndexInTransaction, in_ephemeral, img))
      return false;

    //check that derived key is equal with real output key
    if (!(in_ephemeral.publicKey == src_entr.outputs[src_entr.realOutput].second)) {
      logger(ERROR) << "derived public key mismatch with output public key! " << ENDL << "derived_key:"
        << Common::podToHex(in_ephemeral.publicKey) << ENDL << "real output_public_key:"
        << Common::podToHex(src_entr.outputs[src_entr.realOutput].second);
      return false;
    }

    //put key image into tx input
    KeyInput input_to_key;
    input_to_key.amount = src_entr.amount;
    input_to_key.keyImage = img;

    //fill outputs array and use relative offsets
    for (const TransactionSourceEntry::OutputEntry& out_entry : src_entr.outputs) {
      input_to_key.outputIndexes.push_back(out_entry.first);
    }

    input_to_key.outputIndexes = absolute_output_offsets_to_relative(input_to_key.outputIndexes);
    tx.inputs.push_back(input_to_key);
  }

  KeyPair txkey;
  if (!generateDeterministicTransactionKeys(getObjectHash(tx.inputs), sender_account_keys.viewSecretKey, txkey)) {
    logger(ERROR) << "Couldn't generate deterministic transaction keys";
    return false;
  }

  addTransactionPublicKeyToExtra(tx.extra, txkey.publicKey);

  tx_key = txkey.secretKey;

  // "Shuffle" outs
  std::vector<TransactionDestinationEntry> shuffled_dsts(destinations);
  std::sort(shuffled_dsts.begin(), shuffled_dsts.end(), [](const TransactionDestinationEntry& de1, const TransactionDestinationEntry& de2) { return de1.amount < de2.amount; });

  uint64_t summary_outs_money = 0;
  //fill outputs
  size_t output_index = 0;
  for (const TransactionDestinationEntry& dst_entr : shuffled_dsts) {
    if (!(dst_entr.amount > 0)) {
      logger(ERROR, BRIGHT_RED) << "Destination with wrong amount: " << dst_entr.amount;
      return false;
    }
    KeyDerivation derivation;
    PublicKey out_eph_public_key;
    bool r = generate_key_derivation(dst_entr.addr.viewPublicKey, txkey.secretKey, derivation);

    if (!(r)) {
      logger(ERROR, BRIGHT_RED)
        << "at creation outs: failed to generate_key_derivation("
        << dst_entr.addr.viewPublicKey << ", " << txkey.secretKey << ")";
      return false;
    }

    r = derive_public_key(derivation, output_index,
      dst_entr.addr.spendPublicKey,
      out_eph_public_key);
    if (!(r)) {
      logger(ERROR, BRIGHT_RED)
        << "at creation outs: failed to derive_public_key(" << derivation
        << ", " << output_index << ", " << dst_entr.addr.spendPublicKey
        << ")";
      return false;
    }

    TransactionOutput out;
    out.amount = dst_entr.amount;
    KeyOutput tk;
    tk.key = out_eph_public_key;
    out.target = tk;
    tx.outputs.push_back(out);
    output_index++;
    summary_outs_money += dst_entr.amount;
  }

  //check money
  if (summary_outs_money > summary_inputs_money) {
    logger(ERROR) << "Transaction inputs money (" << summary_inputs_money << ") less than outputs money (" << summary_outs_money << ")";
    return false;
  }

  //generate ring signatures
  Hash tx_prefix_hash;
  getObjectHash(*static_cast<TransactionPrefix*>(&tx), tx_prefix_hash);

  size_t i = 0;
  for (const TransactionSourceEntry& src_entr : sources) {
    std::vector<const PublicKey*> keys_ptrs;
    for (const TransactionSourceEntry::OutputEntry& o : src_entr.outputs) {
      keys_ptrs.push_back(&o.second);
    }

    tx.signatures.push_back(std::vector<Signature>());
    std::vector<Signature>& sigs = tx.signatures.back();
    sigs.resize(src_entr.outputs.size());
    generate_ring_signature(tx_prefix_hash, boost::get<KeyInput>(tx.inputs[i]).keyImage, keys_ptrs,
      in_contexts[i].in_ephemeral.secretKey, src_entr.realOutput, sigs.data());
    i++;
  }

  return true;
}

bool getInputsMoneyAmount(const Transaction& tx, uint64_t& money) {
  money = 0;

  for (const auto& in : tx.inputs) {
    uint64_t amount = 0;

    if (in.type() == typeid(KeyInput)) {
      amount = boost::get<KeyInput>(in).amount;
    } else if (in.type() == typeid(MultisignatureInput)) {
      amount = boost::get<MultisignatureInput>(in).amount;
    }

    money += amount;
  }
  return true;
}

bool checkInputTypesSupported(const TransactionPrefix& tx) {
  for (const auto& in : tx.inputs) {
    if (in.type() != typeid(KeyInput) && in.type() != typeid(MultisignatureInput)) {
      return false;
    }
  }

  return true;
}

bool checkOutsValid(const TransactionPrefix& tx, std::string* error) {
  for (const TransactionOutput& out : tx.outputs) {
    if (out.target.type() == typeid(KeyOutput)) {
      if (out.amount == 0) {
        if (error) {
          *error = "Zero amount ouput";
        }
        return false;
      }

      if (!check_key(boost::get<KeyOutput>(out.target).key)) {
        if (error) {
          *error = "Output with invalid key";
        }
        return false;
      }
    } else if (out.target.type() == typeid(MultisignatureOutput)) {
      const MultisignatureOutput& multisignatureOutput = ::boost::get<MultisignatureOutput>(out.target);
      if (multisignatureOutput.requiredSignatureCount > multisignatureOutput.keys.size()) {
        if (error) {
          *error = "Multisignature output with invalid required signature count";
        }
        return false;
      }
      for (const PublicKey& key : multisignatureOutput.keys) {
        if (!check_key(key)) {
          if (error) {
            *error = "Multisignature output with invalid public key";
          }
          return false;
        }
      }
    } else {
      if (error) {
        *error = "Output with invalid type";
      }
      return false;
    }
  }

  return true;
}

bool checkMultisignatureInputsDiff(const TransactionPrefix& tx) {
  std::set<std::pair<uint64_t, uint32_t>> inputsUsage;
  for (const auto& inv : tx.inputs) {
    if (inv.type() == typeid(MultisignatureInput)) {
      const MultisignatureInput& in = ::boost::get<MultisignatureInput>(inv);
      if (!inputsUsage.insert(std::make_pair(in.amount, in.outputIndex)).second) {
        return false;
      }
    }
  }
  return true;
}

bool checkMoneyOverflow(const TransactionPrefix &tx) {
  return checkInputsOverflow(tx) && checkOutsOverflow(tx);
}

bool checkInputsOverflow(const TransactionPrefix &tx) {
  uint64_t money = 0;

  for (const auto &in : tx.inputs) {
    uint64_t amount = 0;

    if (in.type() == typeid(KeyInput)) {
      amount = boost::get<KeyInput>(in).amount;
    } else if (in.type() == typeid(MultisignatureInput)) {
      amount = boost::get<MultisignatureInput>(in).amount;
    }

    if (money > amount + money)
      return false;

    money += amount;
  }
  return true;
}

bool checkOutsOverflow(const TransactionPrefix& tx) {
  uint64_t money = 0;
  for (const auto& o : tx.outputs) {
    if (money > o.amount + money)
      return false;
    money += o.amount;
  }
  return true;
}

uint64_t get_outs_money_amount(const Transaction& tx) {
  uint64_t outputs_amount = 0;
  for (const auto& o : tx.outputs) {
    outputs_amount += o.amount;
  }
  return outputs_amount;
}

std::string short_hash_str(const Hash& h) {
  std::string res = Common::podToHex(h);

  if (res.size() == 64) {
    auto erased_pos = res.erase(8, 48);
    res.insert(8, "....");
  }

  return res;
}

bool is_out_to_acc(const AccountKeys& acc, const KeyOutput& out_key, const KeyDerivation& derivation, size_t keyIndex) {
  PublicKey pk;
  derive_public_key(derivation, keyIndex, acc.address.spendPublicKey, pk);
  return pk == out_key.key;
}

bool is_out_to_acc(const AccountKeys& acc, const KeyOutput& out_key, const PublicKey& tx_pub_key, size_t keyIndex) {
  KeyDerivation derivation;
  generate_key_derivation(tx_pub_key, acc.viewSecretKey, derivation);
  return is_out_to_acc(acc, out_key, derivation, keyIndex);
}

bool lookup_acc_outs(const AccountKeys& acc, const Transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered) {
  PublicKey transactionPublicKey = getTransactionPublicKeyFromExtra(tx.extra);
  if (transactionPublicKey == NULL_PUBLIC_KEY)
    return false;
  return lookup_acc_outs(acc, tx, transactionPublicKey, outs, money_transfered);
}

bool lookup_acc_outs(const AccountKeys& acc, const Transaction& tx, const PublicKey& tx_pub_key, std::vector<size_t>& outs, uint64_t& money_transfered) {
  money_transfered = 0;
  size_t keyIndex = 0;
  size_t outputIndex = 0;

  KeyDerivation derivation;
  generate_key_derivation(tx_pub_key, acc.viewSecretKey, derivation);

  for (const TransactionOutput& o : tx.outputs) {
    assert(o.target.type() == typeid(KeyOutput) || o.target.type() == typeid(MultisignatureOutput));
    if (o.target.type() == typeid(KeyOutput)) {
      if (is_out_to_acc(acc, boost::get<KeyOutput>(o.target), derivation, keyIndex)) {
        outs.push_back(outputIndex);
        money_transfered += o.amount;
      }

      ++keyIndex;
    } else if (o.target.type() == typeid(MultisignatureOutput)) {
      keyIndex += boost::get<MultisignatureOutput>(o.target).keys.size();
    }

    ++outputIndex;
  }
  return true;
}

bool is_valid_decomposed_amount(uint64_t amount) {
  auto it = std::lower_bound(Currency::PRETTY_AMOUNTS.begin(), Currency::PRETTY_AMOUNTS.end(), amount);
  if (it == Currency::PRETTY_AMOUNTS.end() || amount != *it) {
    return false;
  }
  return true;
}

bool getTransactionProof(const Crypto::Hash& transactionHash, const CryptoNote::AccountPublicAddress& destinationAddress, const Crypto::SecretKey& transactionKey, std::string& transactionProof, Logging::ILogger& log) {
  LoggerRef logger(log, "get_tx_proof");
  Crypto::KeyImage p = *reinterpret_cast<const Crypto::KeyImage*>(&destinationAddress.viewPublicKey);
  Crypto::KeyImage k = *reinterpret_cast<const Crypto::KeyImage*>(&transactionKey);
  Crypto::KeyImage pk = Crypto::scalarmultKey(p, k);
  Crypto::PublicKey R;
  Crypto::PublicKey rA = reinterpret_cast<const PublicKey&>(pk);
  Crypto::secret_key_to_public_key(transactionKey, R);
  Crypto::Signature sig;
  try {
    Crypto::generate_tx_proof(transactionHash, R, destinationAddress.viewPublicKey, rA, transactionKey, sig);
  }
  catch (const std::runtime_error &e) {
    logger(ERROR, BRIGHT_RED) << "Proof generation error: " << *e.what();
    return false;
  }

  transactionProof = Tools::Base58::encode_addr(CryptoNote::parameters::CRYPTONOTE_TX_PROOF_BASE58_PREFIX, 
    std::string((const char *)&rA, sizeof(Crypto::PublicKey)) + std::string((const char *)&sig, sizeof(Crypto::Signature)));

  return true;
}

bool getReserveProof(const std::vector<TransactionOutputInformation>& selectedTransfers, const CryptoNote::AccountKeys& accountKeys, const uint64_t& amount, const std::string& message, std::string& reserveProof, Logging::ILogger& log) {
  LoggerRef logger(log, "get_reserve_proof");

  // compute signature prefix hash
  std::string prefix_data = message;
  prefix_data.append((const char*)&accountKeys.address, sizeof(CryptoNote::AccountPublicAddress));

  std::vector<Crypto::KeyImage> kimages;
  CryptoNote::KeyPair ephemeral;

  // have to repeat this to get key image as we don't store m_key_image
  for (size_t i = 0; i < selectedTransfers.size(); ++i) {
    const TransactionOutputInformation &td = selectedTransfers[i];

    // derive ephemeral secret key
    Crypto::KeyImage ki;
    const bool r = CryptoNote::generate_key_image_helper(accountKeys, td.transactionPublicKey, td.outputInTransaction, ephemeral, ki);
    if (!r) {
      logger(ERROR) << "Failed to generate key image";
      return false;
    }
    // now we can insert key image
    prefix_data.append((const char*)&ki, sizeof(Crypto::PublicKey));
    kimages.push_back(ki);
  }

  Crypto::Hash prefix_hash;
  Crypto::cn_fast_hash(prefix_data.data(), prefix_data.size(), prefix_hash);

  // generate proof entries
  std::vector<reserve_proof_entry> proofs(selectedTransfers.size());

  for (size_t i = 0; i < selectedTransfers.size(); ++i) {
    const TransactionOutputInformation &td = selectedTransfers[i];
    reserve_proof_entry& proof = proofs[i];
    proof.key_image = kimages[i];
    proof.transaction_id = td.transactionHash;
    proof.index_in_transaction = td.outputInTransaction;

    auto txPubKey = td.transactionPublicKey;

    for (int i = 0; i < 2; ++i) {
      Crypto::KeyImage sk = Crypto::scalarmultKey(*reinterpret_cast<const Crypto::KeyImage*>(&txPubKey), *reinterpret_cast<const Crypto::KeyImage*>(&accountKeys.viewSecretKey));
      proof.shared_secret = *reinterpret_cast<const Crypto::PublicKey *>(&sk);

      Crypto::KeyDerivation derivation;
      if (!Crypto::generate_key_derivation(proof.shared_secret, accountKeys.viewSecretKey, derivation)) {
        logger(ERROR) << "Failed to generate key derivation";
        return false;
      }
    }

    // generate signature for shared secret
    Crypto::generate_tx_proof(prefix_hash, accountKeys.address.viewPublicKey, txPubKey, proof.shared_secret, accountKeys.viewSecretKey, proof.shared_secret_sig);

    // derive ephemeral secret key
    Crypto::KeyImage ki;
    CryptoNote::KeyPair ephemeral;

    const bool r = CryptoNote::generate_key_image_helper(accountKeys, td.transactionPublicKey, td.outputInTransaction, ephemeral, ki);
    if (!r) {
      logger(ERROR) << "Failed to generate key image";
      return false;
    }

    if (ephemeral.publicKey != td.outputKey) {
      logger(ERROR) << "Derived public key doesn't agree with the stored one";
      return false;
    }

    // generate signature for key image
    const std::vector<const Crypto::PublicKey *>& pubs = { &ephemeral.publicKey };

    Crypto::generate_ring_signature(prefix_hash, proof.key_image, &pubs[0], 1, ephemeral.secretKey, 0, &proof.key_image_sig);
  }

  // generate signature for the spend key that received those outputs
  Crypto::Signature signature;
  Crypto::generate_signature(prefix_hash, accountKeys.address.spendPublicKey, accountKeys.spendSecretKey, signature);

  // serialize & encode
  reserve_proof p;
  p.proofs.assign(proofs.begin(), proofs.end());
  memcpy(&p.signature, &signature, sizeof(signature));

  BinaryArray ba = toBinaryArray(p);
  std::string ret(ba.begin(), ba.end());

  reserveProof = Tools::Base58::encode_addr(CryptoNote::parameters::CRYPTONOTE_RESERVE_PROOF_BASE58_PREFIX, ret);

  return true;
}

std::string signMessage(const std::string &data, const CryptoNote::AccountKeys &keys) {
  Crypto::Hash hash;
  Crypto::cn_fast_hash(data.data(), data.size(), hash);
  
  Crypto::Signature signature;
  Crypto::generate_signature(hash, keys.address.spendPublicKey, keys.spendSecretKey, signature);
  return Tools::Base58::encode_addr(CryptoNote::parameters::CRYPTONOTE_KEYS_SIGNATURE_BASE58_PREFIX, std::string((const char *)&signature, sizeof(signature)));
}

bool verifyMessage(const std::string &data, const CryptoNote::AccountPublicAddress &address, const std::string &signature, Logging::ILogger& log) {
  LoggerRef logger(log, "verify_message");

  std::string decoded;
  uint64_t prefix;
  if (!Tools::Base58::decode_addr(signature, prefix, decoded) || prefix != CryptoNote::parameters::CRYPTONOTE_KEYS_SIGNATURE_BASE58_PREFIX) {
    logger(Logging::ERROR) << "Signature decoding error";
    return false;
  }

  Crypto::Signature s;
  if (sizeof(s) != decoded.size()) {
    logger(Logging::ERROR) << "Signature size wrong";
    return false;
  }

  Crypto::Hash hash;
  Crypto::cn_fast_hash(data.data(), data.size(), hash);

  memcpy(&s, decoded.data(), sizeof(s));
  return Crypto::check_signature(hash, address.spendPublicKey, s);
}

}
