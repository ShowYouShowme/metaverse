/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 * Copyright (c) 2016-2018 metaverse core developers (see MVS-AUTHORS)
 *
 * This file is part of metaverse.
 *
 * metaverse is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <metaverse/bitcoin/utility/random.hpp>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <random>
#include <boost/thread.hpp>
#include <metaverse/bitcoin/utility/asio.hpp>
#include <metaverse/bitcoin/utility/assert.hpp>
#include <metaverse/bitcoin/utility/data.hpp>
#include <metaverse/bitcoin/utility/thread.hpp>
#include <metaverse/bitcoin/constants.hpp>

namespace libbitcoin {


using namespace bc::asio;
using namespace std::chrono;

// DO NOT USE srand() and rand() on MSVC as srand must be called per thread.
// Values may be truly random depending on the underlying device.

static uint32_t get_clock_seed()
{
    const auto now = high_resolution_clock::now();
    return static_cast<uint32_t>(now.time_since_epoch().count());
}

static std::mt19937& get_twister()
{
    // Boost.thread will clean up the thread statics using this function.
    const auto deleter = [](std::mt19937* twister)
    {
        delete twister;
    };

    // Maintain thread static state space.
    static boost::thread_specific_ptr<std::mt19937> twister(deleter);

    // This is thread safe because the instance is static.
    if (twister.get() == nullptr)
    {
        // Seed with high resolution clock.
        twister.reset(new std::mt19937(get_clock_seed()));
    }

    return *twister;
}

uint64_t pseudo_random()
{
    return pseudo_random(0, max_uint64);
}

uint64_t pseudo_random(uint64_t begin, uint64_t end)
{
    std::uniform_int_distribution<uint64_t> distribution(begin, end);
    return distribution(get_twister());
}

// Not fully testable due to lack of random engine injection.
// This may be truly random depending on the underlying device.
uint64_t nonzero_pseudo_random()
{
    for (auto index = 0; index < 100; ++index)
    {
        const auto value = pseudo_random();
        if (value > 0)
            return value;
    }

    // If above doesn't return something is seriously wrong with the RNG.
    throw std::runtime_error("The RNG produces 100 consecutive zero values.");
}

void pseudo_random_fill(data_chunk& chunk)
{
    // uniform_int_distribution is undefined for sizes < 16 bits.
    std::uniform_int_distribution<uint16_t> distribution(0, max_uint8);

    for (auto& byte: chunk)
        byte = static_cast<uint8_t>(distribution(get_twister()));
}

// Randomly select a time duration in the range:
// [(expiration - expiration / ratio) .. expiration]
// Not fully testable due to lack of random engine injection.
asio::duration pseudo_randomize(const asio::duration& expiration,
    uint8_t ratio)
{
    if (ratio == 0)
        return expiration;

    // Uses milliseconds level resolution.
    const auto max_expire = duration_cast<milliseconds>(expiration).count();

    // [10 secs, 4] => 10000 / 4 => 2500
    const auto limit = max_expire / ratio;

    if (limit == 0)
        return expiration;

    // [0..2^64) % 2500 => [0..2500]
    const auto random_offset = static_cast<int>(pseudo_random(0, limit));

    // (10000 - [0..2500]) => [7500..10000]
    const auto expires = max_expire - random_offset;

    // [7.5..10] second duration.
    return milliseconds(expires);
}

} // namespace libbitcoin
