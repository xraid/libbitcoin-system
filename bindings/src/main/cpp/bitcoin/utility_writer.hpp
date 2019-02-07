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
#ifndef LIBBITCOIN__UTILITY_WRITER_HPP
#define LIBBITCOIN__UTILITY_WRITER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <bitcoin/bitcoin/define.hpp>
//#include <bitcoin/bitcoin/math/hash.hpp>
//#include <bitcoin/bitcoin/utility/data.hpp>
#include <bitcoin/bitcoin/utility/writer.hpp>
#include <bitcoin/p_unsigned_char.hpp>
#include <bitcoin/utility_data_slice.hpp>
#include <bitcoin/math_hash_digest.hpp>
#include <bitcoin/math_short_hash.hpp>
#include <bitcoin/math_mini_hash.hpp>

namespace libbitcoin {
namespace api {

/// Writer interface.
class BC_API utility_writer : public libbitcoin::writer
{
public:
    /// Context.
//    virtual operator bool() const = 0;
//    virtual bool operator!() const = 0;

    /// Write hashes.
    virtual void write_hash(const math_hash_digest& value) = 0;
    virtual void write_short_hash(const math_short_hash& value) = 0;
    virtual void write_mini_hash(const math_mini_hash& value) = 0;

    /// Write big endian integers.
    virtual void write_2_bytes_big_endian(uint16_t value) = 0;
    virtual void write_4_bytes_big_endian(uint32_t value) = 0;
    virtual void write_8_bytes_big_endian(uint64_t value) = 0;
    virtual void write_variable_big_endian(uint64_t value) = 0;
    virtual void write_size_big_endian(size_t value) = 0;

    /// Write little endian integers.
    virtual void write_2_bytes_little_endian(uint16_t value) = 0;
    virtual void write_4_bytes_little_endian(uint32_t value) = 0;
    virtual void write_8_bytes_little_endian(uint64_t value) = 0;
    virtual void write_variable_little_endian(uint64_t value) = 0;
    virtual void write_size_little_endian(size_t value) = 0;

    /// Write one byte.
    virtual void write_byte(uint8_t value) = 0;

    /// Write all bytes.
    virtual void write_bytes(const utility_data_slice data) = 0;

    /// Write required size buffer.
    virtual void write_bytes(const p_unsigned_char data, size_t size) = 0;

    /// Write variable length string.
    virtual void write_string(const std::string& value) = 0;

    /// Write required length string, padded with nulls.
    virtual void write_string(const std::string& value, size_t size) = 0;

    /// Advance iterator without writing.
    virtual void skip(size_t size) = 0;

    virtual ~utility_writer();
};

} // namespace api
} // namespace libbitcoin

#endif
