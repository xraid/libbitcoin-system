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
#ifndef LIBBITCOIN_STEALTH_HPP
#define LIBBITCOIN_STEALTH_HPP

#include <cstdint>
//#include <bitcoin/bitcoin/chain/script.hpp>
#include <bitcoin/bitcoin/compat.hpp>
#include <bitcoin/bitcoin/define.hpp>
#include <bitcoin/math_elliptic_curve.hpp>
#include <bitcoin/utility_binary.hpp>
#include <bitcoin/chain_script.hpp>
#include <bitcoin/p_uint32_t.hpp>

namespace libbitcoin {
namespace api {

/// Determine if the script is a null-data script of at least 32 data bytes.
BC_API bool is_stealth_script(const chain_script& script);

/// Convert a stealth info script to a prefix usable for stealth.
BC_API bool to_stealth_prefix(p_uint32_t out_prefix,
    const chain_script& script);

/// Create a valid stealth ephemeral private key from the provided seed.
BC_API bool create_ephemeral_key(math_ec_secret& out_secret,
    const utility_data_chunk& seed);

/// Create a stealth null data script the specified filter prefix.
/// Create an ephemeral secret key generated from the seed.
BC_API bool create_stealth_data(chain_script& out_null_data,
    math_ec_secret& out_secret, const utility_binary& filter, const utility_data_chunk& seed);

/// Create a stealth null data script the specified filter prefix.
/// Use the ephemeral secret key provided by parameter.
BC_API bool create_stealth_script(chain_script& out_null_data,
    const math_ec_secret& secret, const utility_binary& filter, const utility_data_chunk& seed);

/// Extract the stealth ephemeral public key from an output script.
BC_API bool extract_ephemeral_key(math_ec_compressed& out_ephemeral_public_key,
    const chain_script& script);

/// Extract the unsigned stealth ephemeral public key from an output script.
BC_API bool extract_ephemeral_key(math_hash_digest& out_unsigned_ephemeral_key,
    const chain_script& script);

/// Calculate the shared secret.
BC_API bool shared_secret(math_ec_secret& out_shared, const math_ec_secret& secret,
    const math_ec_compressed& point);

/// Uncover the stealth public key.
BC_API bool uncover_stealth(math_ec_compressed& out_stealth,
    const math_ec_compressed& ephemeral_or_scan, const math_ec_secret& scan_or_ephemeral,
    const math_ec_compressed& spend);

/// Uncover the stealth secret.
BC_API bool uncover_stealth(math_ec_secret& out_stealth,
    const math_ec_compressed& ephemeral_or_scan, const math_ec_secret& scan_or_ephemeral,
    const math_ec_secret& spend);

} // namespace api
} // namespace libbitcoin

#endif
