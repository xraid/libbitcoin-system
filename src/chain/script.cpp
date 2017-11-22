/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/bitcoin/chain/script.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <numeric>
#include <sstream>
#include <utility>
#include <boost/range/adaptor/reversed.hpp>
#include <bitcoin/bitcoin/constants.hpp>
#include <bitcoin/bitcoin/chain/transaction.hpp>
#include <bitcoin/bitcoin/error.hpp>
#include <bitcoin/bitcoin/formats/base_16.hpp>
#include <bitcoin/bitcoin/math/elliptic_curve.hpp>
#include <bitcoin/bitcoin/math/hash.hpp>
#include <bitcoin/bitcoin/machine/interpreter.hpp>
#include <bitcoin/bitcoin/machine/opcode.hpp>
#include <bitcoin/bitcoin/machine/operation.hpp>
#include <bitcoin/bitcoin/machine/rule_fork.hpp>
#include <bitcoin/bitcoin/machine/script_pattern.hpp>
#include <bitcoin/bitcoin/machine/sighash_algorithm.hpp>
#include <bitcoin/bitcoin/message/messages.hpp>
#include <bitcoin/bitcoin/utility/assert.hpp>
#include <bitcoin/bitcoin/utility/container_sink.hpp>
#include <bitcoin/bitcoin/utility/container_source.hpp>
#include <bitcoin/bitcoin/utility/istream_reader.hpp>
#include <bitcoin/bitcoin/utility/ostream_writer.hpp>
#include <bitcoin/bitcoin/utility/string.hpp>

namespace libbitcoin {
namespace chain {

using namespace bc::machine;
using namespace boost::adaptors;

// bit.ly/2cPazSa
static const auto one_hash = hash_literal(
    "0000000000000000000000000000000000000000000000000000000000000001");

// Constructors.
//-----------------------------------------------------------------------------

// A default instance is invalid (until modified).
script::script()
  : cached_(false),
    valid_(false)
{
}

script::script(script&& other)
  : operations_(std::move(other.operations_move())),
    cached_(!operations_.empty()),
    bytes_(std::move(other.bytes_)),
    valid_(other.valid_)
{
}

script::script(const script& other)
  : operations_(other.operations_copy()),
    cached_(!operations_.empty()),
    bytes_(other.bytes_),
    valid_(other.valid_)
{
}

script::script(const operation::list& ops)
{
    from_operations(ops);
}

script::script(operation::list&& ops)
{
    from_operations(std::move(ops));
}

script::script(data_chunk&& encoded, bool prefix)
{
    if (prefix)
    {
        valid_ = from_data(encoded, prefix);
        return;
    }

    // This is an optimization that avoids streaming the encoded bytes.
    bytes_ = std::move(encoded);
    cached_ = false;
    valid_ = true;
}

script::script(const data_chunk& encoded, bool prefix)
{
    valid_ = from_data(encoded, prefix);
}

// Private cache access for move construction.
script::operation::list& script::operations_move()
{
    shared_lock lock(mutex_);
    return operations_;
}

// Private cache access for copy construction.
const script::operation::list& script::operations_copy() const
{
    shared_lock lock(mutex_);
    return operations_;
}

// Operators.
//-----------------------------------------------------------------------------

// Concurrent read/write is not supported, so no critical section.
script& script::operator=(script&& other)
{
    operations_ = other.operations_move();
    cached_ = !operations_.empty();
    bytes_ = std::move(other.bytes_);
    valid_ = other.valid_;
    return *this;
}

// Concurrent read/write is not supported, so no critical section.
script& script::operator=(const script& other)
{
    operations_ = other.operations_copy();
    cached_ = !operations_.empty();
    bytes_ = other.bytes_;
    valid_ = other.valid_;
    return *this;
}

bool script::operator==(const script& other) const
{
    return bytes_ == other.bytes_;
}

bool script::operator!=(const script& other) const
{
    return !(*this == other);
}

// Deserialization.
//-----------------------------------------------------------------------------

// static
script script::factory(const data_chunk& encoded, bool prefix)
{
    script instance;
    instance.from_data(encoded, prefix);
    return instance;
}

// static
script script::factory(std::istream& stream, bool prefix)
{
    script instance;
    instance.from_data(stream, prefix);
    return instance;
}

// static
script script::factory(reader& source, bool prefix)
{
    script instance;
    instance.from_data(source, prefix);
    return instance;
}

bool script::from_data(const data_chunk& encoded, bool prefix)
{
    data_source istream(encoded);
    return from_data(istream, prefix);
}

bool script::from_data(std::istream& stream, bool prefix)
{
    istream_reader source(stream);
    return from_data(source, prefix);
}

// Concurrent read/write is not supported, so no critical section.
bool script::from_data(reader& source, bool prefix)
{
    reset();
    valid_ = true;

    if (prefix)
    {
        const auto size = source.read_size_little_endian();

        // The max_script_size constant limits evaluation, but not all scripts
        // evaluate, so use max_block_size to guard memory allocation here.
        if (size > max_block_size)
            source.invalidate();
        else
            bytes_ = source.read_bytes(size);
    }
    else
    {
        bytes_ = source.read_bytes();
    }

    if (!source)
        reset();

    return source;
}

// Concurrent read/write is not supported, so no critical section.
bool script::from_string(const std::string& mnemonic)
{
    reset();

    // There is strictly one operation per string token.
    const auto tokens = split(mnemonic);
    operation::list ops;
    ops.resize(tokens.empty() || tokens.front().empty() ? 0 : tokens.size());

    // Create an op list from the split tokens, one operation per token.
    for (size_t index = 0; index < ops.size(); ++index)
        if (!ops[index].from_string(tokens[index]))
            return false;

    from_operations(ops);
    return true;
}

// Concurrent read/write is not supported, so no critical section.
void script::from_operations(operation::list&& ops)
{
    ////reset();
    bytes_ = operations_to_data(ops);
    operations_ = std::move(ops);
    cached_ = true;
    valid_ = true;
}

// Concurrent read/write is not supported, so no critical section.
void script::from_operations(const operation::list& ops)
{
    ////reset();
    bytes_ = operations_to_data(ops);
    operations_ = ops;
    cached_ = true;
    valid_ = true;
}

// private/static
data_chunk script::operations_to_data(const operation::list& ops)
{
    data_chunk out;
    const auto size = serialized_size(ops);
    out.reserve(size);
    const auto concatenate = [&out](const operation& op)
    {
        auto bytes = op.to_data(false);
        std::move(bytes.begin(), bytes.end(), std::back_inserter(out));
    };

    std::for_each(ops.begin(), ops.end(), concatenate);
    BITCOIN_ASSERT(out.size() == size);
    return out;
}

// private/static
size_t script::serialized_size(const operation::list& ops)
{
    const auto op_size = [](size_t total, const operation& op)
    {
        return total + op.serialized_size(false);
    };

    return std::accumulate(ops.begin(), ops.end(), size_t{0}, op_size);
}

// protected
// Concurrent read/write is not supported, so no critical section.
void script::reset()
{
    bytes_.clear();
    bytes_.shrink_to_fit();
    valid_ = false;
    cached_ = false;
    operations_.clear();
    operations_.shrink_to_fit();
}

bool script::is_valid() const
{
    // All script bytes are valid under some circumstance (e.g. coinbase).
    // This returns false if a prefix and byte count does not match.
    return valid_;
}

bool script::is_valid_operations() const
{
    // Script validity is independent of individual operation validity.
    // There is a trailing invalid/default op if a push op had a size mismatch.
    return operations().empty() || operations_.back().is_valid();
}

// Serialization.
//-----------------------------------------------------------------------------

data_chunk script::to_data(bool prefix) const
{
    data_chunk data;
    const auto size = serialized_size(prefix);
    data.reserve(size);
    data_sink ostream(data);
    to_data(ostream, prefix);
    ostream.flush();
    BITCOIN_ASSERT(data.size() == size);
    return data;
}

void script::to_data(std::ostream& stream, bool prefix) const
{
    ostream_writer sink(stream);
    to_data(sink, prefix);
}

void script::to_data(writer& sink, bool prefix) const
{
    // TODO: optimize by always storing the prefixed serialization.
    if (prefix)
        sink.write_variable_little_endian(serialized_size(false));

    sink.write_bytes(bytes_);
}

std::string script::to_string(uint32_t active_forks) const
{
    auto first = true;
    std::ostringstream text;

    for (const auto& op: operations())
    {
        text << (first ? "" : " ") << op.to_string(active_forks);
        first = false;
    }

    // An invalid operation has a specialized serialization.
    return text.str();
}

// Iteration.
//-----------------------------------------------------------------------------
// These are syntactic sugar that allow the caller to iterate ops directly.
// The first operations access must be method-based to guarantee the cache.

void script::clear()
{
    reset();
}

bool script::empty() const
{
    return operations().empty();
}

size_t script::size() const
{
    return operations().size();
}

const operation& script::front() const
{
    BITCOIN_ASSERT(!operations().empty());
    return operations().front();
}

const operation& script::back() const
{
    BITCOIN_ASSERT(!operations().empty());
    return operations().back();
}

const operation& script::operator[](size_t index) const
{
    BITCOIN_ASSERT(index < operations().size());
    return operations()[index];
}

operation::iterator script::begin() const
{
    return operations().begin();
}

operation::iterator script::end() const
{
    return operations().end();
}

// Properties (size, accessors, cache).
//-----------------------------------------------------------------------------

size_t script::serialized_size(bool prefix) const
{
    auto size = bytes_.size();

    if (prefix)
        size += message::variable_uint_size(size);

    return size;
}

// protected
const operation::list& script::operations() const
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (cached_)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return operations_;
    }

    operation op;
    data_source istream(bytes_);
    istream_reader source(istream);
    const auto size = bytes_.size();

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    mutex_.unlock_upgrade_and_lock();

    // One operation per byte is the upper limit of operations.
    operations_.reserve(size);

    // ************************************************************************
    // CONSENSUS: In the case of a coinbase script we must parse the entire
    // script, beyond just the BIP34 requirements, so that sigops can be
    // calculated from the script. These are counted despite being irrelevant.
    // In this case an invalid script is parsed to the extent possible.
    // ************************************************************************

    // If an op fails it is pushed to operations and the loop terminates.
    // To validate the ops the caller must test the last op.is_valid(), or may
    // text script.is_valid_operations(), which is done in script validation.
    while (!source.is_exhausted())
    {
        op.from_data(source);
        operations_.push_back(std::move(op));
    }

    operations_.shrink_to_fit();
    cached_ = true;

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return operations_;
}

// Signing.
//-----------------------------------------------------------------------------

inline sighash_algorithm to_sighash_enum(uint8_t sighash_type)
{
    switch (sighash_type & sighash_algorithm::mask)
    {
        case sighash_algorithm::single:
            return sighash_algorithm::single;
        case sighash_algorithm::none:
            return sighash_algorithm::none;
        default:
            return sighash_algorithm::all;
    }
}

inline uint8_t is_sighash_enum(uint8_t sighash_type, sighash_algorithm value)
{
    return to_sighash_enum(sighash_type) == value;
}

static hash_digest sign_none(const transaction& tx, uint32_t input_index,
    const script& script_code, uint8_t sighash_type)
{
    input::list ins;
    const auto& inputs = tx.inputs();
    const auto any = (sighash_type & sighash_algorithm::anyone_can_pay) != 0;
    ins.reserve(any ? 1 : inputs.size());

    BITCOIN_ASSERT(input_index < inputs.size());
    const auto& self = inputs[input_index];

    if (any)
    {
        // Retain only self.
        ins.emplace_back(self.previous_output(), script_code, self.sequence());
    }
    else
    {
        // Erase all input scripts and sequences.
        for (const auto& input: inputs)
            ins.emplace_back(input.previous_output(), script{}, 0);

        // Replace self that is lost in the loop.
        ins[input_index].set_script(script_code);
        ins[input_index].set_sequence(self.sequence());
    }

    // Move new inputs to new transaction and drop outputs.
    return transaction(tx.version(), tx.locktime(), std::move(ins),
        output::list{}).hash(sighash_type, false);
}

static hash_digest sign_single(const transaction& tx, uint32_t input_index,
    const script& script_code, uint8_t sighash_type)
{
    input::list ins;
    const auto& inputs = tx.inputs();
    const auto any = (sighash_type & sighash_algorithm::anyone_can_pay) != 0;
    ins.reserve(any ? 1 : inputs.size());

    BITCOIN_ASSERT(input_index < inputs.size());
    const auto& self = inputs[input_index];

    if (any)
    {
        // Retain only self.
        ins.emplace_back(self.previous_output(), script_code, self.sequence());
    }
    else
    {
        // Erase all input scripts and sequences.
        for (const auto& input: inputs)
            ins.emplace_back(input.previous_output(), script{}, 0);

        // Replace self that is lost in the loop.
        ins[input_index].set_script(script_code);
        ins[input_index].set_sequence(self.sequence());
    }

    // Trim and clear outputs except that of specified input index.
    const auto& outputs = tx.outputs();
    output::list outs(input_index + 1);

    BITCOIN_ASSERT(input_index < outputs.size());
    outs.back() = outputs[input_index];

    // Move new inputs and new outputs to new transaction.
    return transaction(tx.version(), tx.locktime(), std::move(ins),
        std::move(outs)).hash(sighash_type, false);
}

static hash_digest sign_all(const transaction& tx, uint32_t input_index,
    const script& script_code, uint8_t sighash_type)
{
    input::list ins;
    const auto& inputs = tx.inputs();
    const auto any = (sighash_type & sighash_algorithm::anyone_can_pay) != 0;
    ins.reserve(any ? 1 : inputs.size());

    BITCOIN_ASSERT(input_index < inputs.size());
    const auto& self = inputs[input_index];

    if (any)
    {
        // Retain only self.
        ins.emplace_back(self.previous_output(), script_code, self.sequence());
    }
    else
    {
        // Erase all input scripts.
        for (const auto& input: inputs)
            ins.emplace_back(input.previous_output(), script{},
                input.sequence());

        // Replace self that is lost in the loop.
        ins[input_index].set_script(script_code);
        ////ins[input_index].set_sequence(self.sequence());
    }

    // Move new inputs and copy outputs to new transaction.
    transaction out(tx.version(), tx.locktime(), input::list{}, tx.outputs());
    out.set_inputs(std::move(ins));
    return out.hash(sighash_type, false);
}

static script strip_code_seperators(const script& script_code)
{
    operation::list ops;

    for (auto op = script_code.begin(); op != script_code.end(); ++op)
        if (op->code() != opcode::codeseparator)
            ops.push_back(*op);

    return script(std::move(ops));
}

// static
// Use bool for version && bip143.
hash_digest script::generate_signature_hash(const transaction& tx,
    uint32_t input_index, const script& script_code, uint8_t sighash_type)
{
    const auto sighash = to_sighash_enum(sighash_type);

    if (input_index >= tx.inputs().size() ||
        (input_index >= tx.outputs().size() &&
            sighash == sighash_algorithm::single))
    {
        //*********************************************************************
        // CONSENSUS: wacky satoshi behavior we must perpetuate.
        //*********************************************************************
        return one_hash;
    }

    //*************************************************************************
    // CONSENSUS: more wacky satoshi behavior we must perpetuate.
    //*************************************************************************
    const auto stripped = strip_code_seperators(script_code);

    // The sighash serializations are isolated for clarity and optimization.
    switch (sighash)
    {
        case sighash_algorithm::none:
            return sign_none(tx, input_index, stripped, sighash_type);
        case sighash_algorithm::single:
            return sign_single(tx, input_index, stripped, sighash_type);
        default:
        case sighash_algorithm::all:
            return sign_all(tx, input_index, stripped, sighash_type);
    }
}

// static
bool script::check_signature(const ec_signature& signature,
    uint8_t sighash_type, const data_chunk& public_key,
    const script& script_code, const transaction& tx, uint32_t input_index)
{
    if (public_key.empty())
        return false;

    // This always produces a valid signature hash, including one_hash.
    const auto sighash = script::generate_signature_hash(tx, input_index,
        script_code, sighash_type);

    // Validate the EC signature.
    return verify_signature(public_key, sighash, signature);
}

// static
bool script::create_endorsement(endorsement& out, const ec_secret& secret,
    const script& prevout_script, const transaction& tx, uint32_t input_index,
    uint8_t sighash_type)
{
    out.reserve(max_endorsement_size);

    // This always produces a valid signature hash, including one_hash.
    const auto sighash = script::generate_signature_hash(tx, input_index,
        prevout_script, sighash_type);

    // Create the EC signature and encode as DER.
    ec_signature signature;
    if (!sign(signature, secret, sighash) || !encode_signature(out, signature))
        return false;

    // Add the sighash type to the end of the DER signature -> endorsement.
    out.push_back(sighash_type);
    out.shrink_to_fit();
    return true;
}

// Utilities (static).
//-----------------------------------------------------------------------------

bool script::is_push_only(const operation::list& ops)
{
    const auto push = [](const operation& op)
    {
        return op.is_push();
    };

    return std::all_of(ops.begin(), ops.end(), push);
}

//*****************************************************************************
// CONSENSUS: this pattern is used to activate bip16 validation rules.
//*****************************************************************************
bool script::is_relaxed_push(const operation::list& ops)
{
    const auto push = [&](const operation& op)
    {
        return op.is_relaxed_push();
    };

    return std::all_of(ops.begin(), ops.end(), push);
}

//*****************************************************************************
// CONSENSUS: BIP34 requires coinbase input script to begin with one byte that
// indicates the height size. This is inconsistent with an extreme future where
// the size byte overflows. However satoshi actually requires nominal encoding.
//*****************************************************************************
bool script::is_coinbase_pattern(const operation::list& ops, size_t height)
{
    return !ops.empty()
        && ops[0].is_nominal_push()
        && ops[0].data() == number(height).data();
}

bool script::is_commitment_pattern(const operation::list& ops)
{
    static const auto header = to_big_endian(witness_head);

    // Bytes after commitment are optional with no consensus meaning (bip141).
    // Commitment is not executable so invalid trailing operations are allowed.
    return ops.size() > 1
        && ops[0].code() == opcode::return_
        && ops[1].code() == opcode::push_size_36
        && std::equal(header.begin(), header.end(), ops[1].data().begin());
}

// The satoshi client tests for 83 bytes total. This allows for the waste of
// one byte to represent up to 75 bytes using the push_one_size opcode.
// It also allows any number of push ops and limits it to 0 value and 1 per tx.
////bool script::is_pay_null_data_pattern(const operation::list& ops)
////{
////    static constexpr auto op_76 = static_cast<uint8_t>(opcode::push_one_size);
////
////    return ops.size() >= 2
////        && ops[0].code() == opcode::return_
////        && static_cast<uint8_t>(ops[1].code()) <= op_76
////        && ops[1].data().size() <= max_null_data_size;
////}

// The satoshi client enables configurable data size for policy.
bool script::is_pay_null_data_pattern(const operation::list& ops)
{
    return ops.size() == 2
        && ops[0].code() == opcode::return_
        && ops[1].is_minimal_push()
        && ops[1].data().size() <= max_null_data_size;
}

// TODO: expand this to the 20 signature op_check_multisig limit.
// The current 16 (or 20) limit does not affect server indexing because bare
// multisig is not indexable and p2sh multisig is byte-limited to 15 sigs.
// The satoshi client policy limit is 3 signatures for bare multisig.
bool script::is_pay_multisig_pattern(const operation::list& ops)
{
    static constexpr auto op_1 = static_cast<uint8_t>(opcode::push_positive_1);
    static constexpr auto op_16 = static_cast<uint8_t>(opcode::push_positive_16);

    const auto op_count = ops.size();

    if (op_count < 4 || ops[op_count - 1].code() != opcode::checkmultisig)
        return false;

    const auto op_m = static_cast<uint8_t>(ops[0].code());
    const auto op_n = static_cast<uint8_t>(ops[op_count - 2].code());

    if (op_m < op_1 || op_m > op_n || op_n < op_1 || op_n > op_16)
        return false;

    const auto number = op_n - op_1 + 1u;
    const auto points = op_count - 3u;

    if (number != points)
        return false;

    for (auto op = ops.begin() + 1; op != ops.end() - 2; ++op)
        if (!is_public_key(op->data()))
            return false;

    return true;
}

// The satoshi client considers this non-standard for policy.
bool script::is_pay_public_key_pattern(const operation::list& ops)
{
    return ops.size() == 2
        && is_public_key(ops[0].data())
        && ops[1].code() == opcode::checksig;
}

bool script::is_pay_key_hash_pattern(const operation::list& ops)
{
    return ops.size() == 5
        && ops[0].code() == opcode::dup
        && ops[1].code() == opcode::hash160
        && ops[2].data().size() == short_hash_size
        && ops[3].code() == opcode::equalverify
        && ops[4].code() == opcode::checksig;
}

//*****************************************************************************
// CONSENSUS: this pattern is used to activate bip16 validation rules.
//*****************************************************************************
bool script::is_pay_script_hash_pattern(const operation::list& ops)
{
    return ops.size() == 3
        && ops[0].code() == opcode::hash160
        && ops[1].code() == opcode::push_size_20
        && ops[1].data().size() == short_hash_size
        && ops[2].code() == opcode::equal;
}

// The first push is based on wacky satoshi op_check_multisig behavior that
// we must perpetuate, though it's appearance here is policy not consensus.
// Limiting to push_size_0 eliminates pattern ambiguity with little downside.
bool script::is_sign_multisig_pattern(const operation::list& ops)
{
    return ops.size() >= 2
        && ops[0].code() == opcode::push_size_0
        && std::all_of(ops.begin() + 1, ops.end(), [](const operation& op)
            { return is_endorsement(op.data()); });
}

bool script::is_sign_public_key_pattern(const operation::list& ops)
{
    return ops.size() == 1
        && is_endorsement(ops[0].data());
}

bool script::is_sign_key_hash_pattern(const operation::list& ops)
{
    return ops.size() == 2
        && is_endorsement(ops[0].data())
        && is_public_key(ops[1].data());
}

// Ambiguous with is_sign_key_hash when second/last op is a public key.
// Ambiguous with is_sign_public_key_pattern when only op is an endorsement.
bool script::is_sign_script_hash_pattern(const operation::list& ops)
{
    return !ops.empty()
        && is_push_only(ops)
        && !ops.back().data().empty();
}

operation::list script::to_pay_null_data_pattern(data_slice data)
{
    if (data.size() > max_null_data_size)
        return{};

    return operation::list
    {
        { opcode::return_ },
        { to_chunk(data) }
    };
}

operation::list script::to_pay_public_key_pattern(data_slice point)
{
    if (!is_public_key(point))
        return{};

    return operation::list
    {
        { to_chunk(point) },
        { opcode::checksig }
    };
}

operation::list script::to_pay_key_hash_pattern(const short_hash& hash)
{
    return operation::list
    {
        { opcode::dup },
        { opcode::hash160 },
        { to_chunk(hash) },
        { opcode::equalverify },
        { opcode::checksig }
    };
}

operation::list script::to_pay_script_hash_pattern(const short_hash& hash)
{
    return operation::list
    {
        { opcode::hash160 },
        { to_chunk(hash) },
        { opcode::equal }
    };
}

operation::list script::to_pay_multisig_pattern(uint8_t signatures,
    const point_list& points)
{
    data_stack chunks;
    chunks.reserve(points.size());
    const auto conversion = [&chunks](const ec_compressed& point)
    {
        chunks.push_back(to_chunk(point));
    };

    // Operation ordering matters, don't use std::transform here.
    std::for_each(points.begin(), points.end(), conversion);
    return to_pay_multisig_pattern(signatures, chunks);
}

// TODO: expand this to a 20 signature limit.
// This supports up to 16 signatures, however check_multisig is limited to 20.
// The embedded script is limited to 520 bytes, an effective limit of 15 for
// p2sh multisig, which can be as low as 7 when using all uncompressed keys.
operation::list script::to_pay_multisig_pattern(uint8_t signatures,
    const data_stack& points)
{
    static constexpr auto op_81 = static_cast<uint8_t>(opcode::push_positive_1);
    static constexpr auto op_96 = static_cast<uint8_t>(opcode::push_positive_16);
    static constexpr auto zero = op_81 - 1;
    static constexpr auto max = op_96 - zero;

    const auto m = signatures;
    const auto n = points.size();

    if (m < 1 || m > n || n < 1 || n > max)
        return operation::list();

    const auto op_m = static_cast<opcode>(m + zero);
    const auto op_n = static_cast<opcode>(points.size() + zero);

    operation::list ops;
    ops.reserve(points.size() + 3);
    ops.emplace_back(op_m);

    for (const auto point: points)
    {
        if (!is_public_key(point))
            return{};

        ops.emplace_back(point);
    }

    ops.emplace_back(op_n);
    ops.emplace_back(opcode::checkmultisig);
    return ops;
}

// Utilities (non-static).
//-----------------------------------------------------------------------------

// Caller should test for is_sign_script_hash_pattern when sign_key_hash result
// as it is possible for an input script to match both patterns.
script_pattern script::pattern() const
{
    const auto input = output_pattern();
    return input == script_pattern::non_standard ? input_pattern() : input;
}

// Output patterns are mutually and input unambiguous.
// The bip141 coinbase pattern is not tested here, must test independently.
script_pattern script::output_pattern() const
{
    // The first operations access must be method-based to guarantee the cache.
    if (is_pay_key_hash_pattern(operations()))
        return script_pattern::pay_key_hash;

    if (is_pay_script_hash_pattern(operations_))
        return script_pattern::pay_script_hash;

    if (is_pay_null_data_pattern(operations_))
        return script_pattern::pay_null_data;

    if (is_pay_public_key_pattern(operations_))
        return script_pattern::pay_public_key;

    if (is_pay_multisig_pattern(operations_))
        return script_pattern::pay_multisig;

    return script_pattern::non_standard;
}

// A sign_key_hash result always implies sign_script_hash as well.
// The bip34 coinbase pattern is not tested here, must test independently.
script_pattern script::input_pattern() const
{
    // The first operations access must be method-based to guarantee the cache.
    if (is_sign_key_hash_pattern(operations()))
        return script_pattern::sign_key_hash;

    // This must follow is_sign_key_hash_pattern for ambiguity comment to hold.
    if (is_sign_script_hash_pattern(operations_))
        return script_pattern::sign_script_hash;

    if (is_sign_public_key_pattern(operations_))
        return script_pattern::sign_public_key;

    if (is_sign_multisig_pattern(operations_))
        return script_pattern::sign_multisig;

    return script_pattern::non_standard;
}

bool script::is_pay_to_script_hash(uint32_t forks) const
{
    // This is used internally as an optimization over using script::pattern.
    // The prevout operations access must be method-based to guarantee the cache.
    return is_enabled(forks, rule_fork::bip16_rule) &&
        is_pay_script_hash_pattern(operations());
}

inline size_t ops(bool embedded, opcode code)
{
    return embedded && operation::is_positive(code) ?
        operation::opcode_to_positive(code) : multisig_default_sigops;
}

size_t script::sigops(bool embedded) const
{
    size_t total = 0;
    auto preceding = opcode::push_negative_1;

    // The first operations access must be method-based to guarantee the cache.
    for (const auto& op: operations())
    {
        const auto code = op.code();

        if (code == opcode::checksig ||
            code == opcode::checksigverify)
        {
            ++total;
        }
        else if (
            code == opcode::checkmultisig ||
            code == opcode::checkmultisigverify)
        {
            total += ops(embedded, preceding);
        }

        preceding = code;
    }

    return total;
}

size_t script::embedded_sigops(const script& prevout_script) const
{
    // There are no embedded sigops when the prevout script is not p2sh.
    if (!prevout_script.is_pay_to_script_hash(rule_fork::bip16_rule))
        return 0;

    // The first operations access must be method-based to guarantee the cache.
    if (operations().empty())
        return 0;

    // There are no embedded sigops when the input script is not push only.
    if (!is_relaxed_push(operations_))
        return 0;

    // Parse the embedded script from the last input script item (data).
    // This never fails because there is no prefix to validate the length.
    script embedded(operations_.back().data(), false);

    // Count the sigops in the embedded script using BIP16 rules.
    return embedded.sigops(true);
}

//*****************************************************************************
// CONSENSUS: this is a pointless, broken, premature optimization attempt.
// The comparison and erase are not limited to a single operation and so can
// erase arbitrary upstream data from the script.
//*****************************************************************************
void script::find_and_delete_(const data_chunk& endorsement)
{
    // If this is empty it would produce an empty script but not operation.
    // So we test it for empty prior to operation reserialization.
    if (endorsement.empty())
        return;

    // The value must be serialized to script using non-minimal encoding.
    // Non-minimally-encoded target values will therefore not match.
    const auto value = operation(endorsement, false).to_data();

    operation op;
    data_source stream(bytes_);
    istream_reader source(stream);
    std::vector<data_chunk::iterator> found;

    // The exhaustion test handles stream end and op deserialization failure.
    for (auto it = bytes_.begin(); !source.is_exhausted();
        it += op.serialized_size())
    {
        // Track all found values for later deletion.
        for (; starts_with(it, bytes_.end(), value); it += value.size())
        {
            source.skip(value.size());
            found.push_back(it);
        }

        // Read the next op code following last found value.
        op.from_data(source);
    }

    // Delete any found values, reversed to prevent iterator invalidation.
    for (const auto it: reverse(found))
        bytes_.erase(it, it + value.size());
}

// Concurrent read/write is not supported, so no critical section.
void script::find_and_delete(const data_stack& endorsements)
{
    for (auto& endorsement: endorsements)
        find_and_delete_(endorsement);

    // Invalidate the cache so that the operations may be regenerated.
    operations_.clear();
    cached_ = false;
    bytes_.shrink_to_fit();
}

////// This is slightly more efficient because the script does not get parsed,
////// but the static template implementation is more self-explanatory.
////bool script::is_coinbase_pattern(size_t height) const
////{
////    const auto actual = to_data(false);
////
////    // Create the expected script as a non-minimal byte vector.
////    script compare(operation::list{ { number(height).data(), false } });
////    const auto expected = compare.to_data(false);
////
////    // Require the actual script start with the expected coinbase script.
////    return std::equal(expected.begin(), expected.end(), actual.begin());
////}

// An unspendable script is any that can provably not be spent under any
// circumstance. This allows for exclusion of the output as unspendable.
// The criteria below are not be comprehensive but are fast to evaluate.
bool script::is_unspendable() const
{
    // The first operations access must be method-based to guarantee the cache.
    return (!operations().empty() && operations_[0].code() == opcode::return_)
        || serialized_size(false) > max_script_size;
}

// Validation.
//-----------------------------------------------------------------------------

code script::verify(const transaction& tx, uint32_t input_index,
    uint32_t forks, const script& input_script, const script& prevout_script)
{
    code ec;

    program input(input_script, tx, input_index, forks);
    if ((ec = input.evaluate()))
        return ec;

    program prevout(prevout_script, input);
    if ((ec = prevout.evaluate()))
        return ec;

    if (!prevout.stack_result())
        return error::stack_false;

    if (prevout_script.is_pay_to_script_hash(forks))
    {
        if (!is_relaxed_push(input_script.operations()))
            return error::invalid_script_embed;

        // The embedded p2sh script is at the top of the stack.
        script embedded_script(input.pop(), false);

        program embedded(embedded_script, std::move(input), true);
        if ((ec = embedded.evaluate()))
            return ec;

        if (!embedded.stack_result())
            return error::stack_false;
    }

    return error::success;
}

code script::verify(const transaction& tx, uint32_t input, uint32_t forks)
{
    if (input >= tx.inputs().size())
        return error::operation_failed;

    const auto& in = tx.inputs()[input];
    const auto& prevout = in.previous_output().validation.cache;
    return verify(tx, input, forks, in.script(), prevout.script());
}

} // namespace chain
} // namespace libbitcoin
