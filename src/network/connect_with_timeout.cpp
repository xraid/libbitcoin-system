/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "connect_with_timeout.hpp"

#include <functional>
#include <memory>
#include <system_error>
#include <boost/asio.hpp>
#include <boost/date_time.hpp>
#include <bitcoin/bitcoin/error.hpp>
#include <bitcoin/bitcoin/utility/assert.hpp>
#include <bitcoin/bitcoin/network/channel.hpp>
#include <bitcoin/bitcoin/network/channel_proxy.hpp>
#include <bitcoin/bitcoin/network/network.hpp>
#include <bitcoin/bitcoin/utility/logger.hpp>
#include <bitcoin/bitcoin/utility/threadpool.hpp>

namespace libbitcoin {
namespace network {

using std::placeholders::_1;
using std::placeholders::_2;
using boost::asio::ip::tcp;
using boost::posix_time::time_duration;

connect_with_timeout::connect_with_timeout(threadpool& pool,
    const timeout& timeouts)
  : timer_(pool.service()),
    connect_timeout_(timeouts.connect),
    socket_(std::make_shared<tcp::socket>(pool.service())),
    proxy_(std::make_shared<channel_proxy>(pool, socket_, timeouts))
{
}

void connect_with_timeout::start(tcp::resolver::iterator endpoint_iterator,
    network::connect_handler handle_connect)
{
    timer_.expires_from_now(connect_timeout_);
    timer_.async_wait(
        std::bind(&connect_with_timeout::handle_timer,
            shared_from_this(), _1));

    boost::asio::async_connect(*socket_, endpoint_iterator,
        std::bind(&connect_with_timeout::call_handle_connect,
            shared_from_this(), _1, _2, handle_connect));
}

void connect_with_timeout::call_handle_connect(
    const boost::system::error_code& ec, tcp::resolver::iterator, 
    network::connect_handler handle_connect)
{
    timer_.cancel();

    if (ec)
    {
        handle_connect(error::boost_to_error_code(ec), nullptr);
        return;
    }

    const auto channel_object = std::make_shared<channel>(proxy_);
    handle_connect(error::success, channel_object);
    proxy_->start();
}

void connect_with_timeout::handle_timer(const boost::system::error_code& ec)
{
    if (timeout::canceled(ec))
        return;

    // If there is no error the timer fired because of expiration.
    auto code = error::boost_to_error_code(ec);
    if (!code)
    {
        code = error::channel_timeout;

        // TODO: get address from query.
        //log_debug(LOG_NETWORK)
        //    << "Timeout connecting to [" << address << "] ";
    }

    proxy_->stop(code);
}

} // namespace network
} // namespace libbitcoin