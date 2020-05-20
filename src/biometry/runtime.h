/*
 * Copyright (C) 2016 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 *
 */

#ifndef BIOMETRYD_RUNTIME_H_
#define BIOMETRYD_RUNTIME_H_

#include <biometry/visibility.h>

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <cstdint>

namespace biometry
{
// We bundle our "global" runtime dependencies here, specifically
// a dispatcher to decouple multiple in-process providers from one
// another , forcing execution to a well known set of threads.
class BIOMETRY_DLL_PUBLIC Runtime : public std::enable_shared_from_this<Runtime>
{
public:
    // Our default concurrency setup.
    static constexpr const std::uint32_t worker_threads = 2;

    // create returns a Runtime instance with pool_size worker threads
    // executing the underlying service.
    static std::shared_ptr<Runtime> create(std::uint32_t pool_size = worker_threads);

    Runtime(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    // Tears down the runtime, stopping all worker threads.
    ~Runtime() noexcept(true);
    Runtime& operator=(const Runtime&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    // start executes the underlying io_context on a thread pool with
    // the size configured at creation time.
    void start();

    // stop cleanly shuts down a Runtime instance,
    // joining all worker threads.
    void stop();

    // to_dispatcher_functional returns a function for integration
    // with components that expect a dispatcher for operation.
    std::function<void(std::function<void()>)> to_dispatcher_functional();

    // service returns the underlying boost::asio::io_context that is executed
    // by the Runtime.
    boost::asio::io_context& service();

private:
    // Runtime constructs a new instance, firing up pool_size
    // worker threads.
    Runtime(std::uint32_t pool_size);

    std::uint32_t pool_size_;
    boost::asio::io_context service_;
    boost::asio::io_context::strand strand_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> keep_alive_;
    std::vector<std::thread> workers_;
};
}

#endif // BIOMETRYD_RUNTIME_H_
