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
#ifndef LIBBITCOIN__MACHINE_SCRIPT_VERSION_HPP
#define LIBBITCOIN__MACHINE_SCRIPT_VERSION_HPP

#include <bitcoin/bitcoin/machine/script_version.hpp>

namespace libbitcoin {
//namespace machine {
namespace api {

/// Script versions (bip141).
enum class machine_script_version : machine::script_version
{
    /// Defined by bip141.
    zero,

    /// All reserved script versions (1..16).
    reserved,

    /// All unversioned scripts.
    unversioned
};

} // namespace machine
//} // namespace machine
} // namespace libbitcoin

#endif
