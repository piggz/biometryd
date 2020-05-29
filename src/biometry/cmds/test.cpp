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

#include <biometry/cmds/test.h>

#include <biometry/application.h>
#include <biometry/device_registry.h>
#include <biometry/identifier.h>
#include <biometry/reason.h>
#include <biometry/service.h>
#include <biometry/template_store.h>
#include <biometry/tracing_operation_observer.h>
#include <biometry/user.h>

#include <biometry/dbus/service.h>

#include <biometry/util/benchmark.h>
#include <biometry/util/configuration.h>
#include <biometry/util/json_configuration_builder.h>
#include <biometry/util/streaming_configuration_builder.h>

#include <iomanip>
#include <future>
#include <stdexcept>

namespace cli = biometry::util::cli;

namespace
{
template<typename T>
class SyncingObserver : public biometry::Operation<T>::Observer
{
public:
    typedef typename biometry::Operation<T>::Observer Super;

    SyncingObserver(std::ostream& out, const std::string& name, std::uint32_t width = 80)
        : pb{out, name, width},
          future{promise.get_future()}
    {
      ctxt.cout << "SyncingObserver constructed" << std::endl;
    }

    typename Super::Result sync()
    {
      if (!future.valid()) 
        ctxt.cout << "future invalid" << std::endl;
      return future.get();
    }

    // From biometry::Operation<T>::Observer
    void on_started() override
    {
        pb.update(0);
    }

    void on_progress(const typename Super::Progress& progress) override
    {
        pb.update(*progress.percent);
    }

    void on_canceled(const typename Super::Reason& reason) override
    {
        promise.set_exception(std::make_exception_ptr(std::runtime_error{reason}));
    }

    void on_failed(const typename Super::Error& error) override
    {
        promise.set_exception(std::make_exception_ptr(std::runtime_error{error}));
    }

    void on_succeeded(const typename Super::Result& result) override
    {
        promise.set_value(result);
    }

private:
    biometry::util::cli::ProgressBar pb;
    std::promise<typename Super::Result> promise;
    std::future<typename Super::Result> future; //{promise.get_future()};
};

std::shared_ptr<biometry::Device> device_from_config_file(const boost::filesystem::path& file)
{
    using StreamingJsonConfigurationBuilder = biometry::util::StreamingConfigurationBuilder<biometry::util::JsonConfigurationBuilder>;
    StreamingJsonConfigurationBuilder builder{StreamingJsonConfigurationBuilder::make_streamer(file)};

    const auto configuration = builder.build_configuration();

    static const auto throw_configuration_invalid = [](){ std::throw_with_nested(biometry::cmds::Test::ConfigurationInvalid{});};

    const auto id = configuration
            ("device",  throw_configuration_invalid)
            ("id",      throw_configuration_invalid);
    biometry::util::Configuration config; config["config"] = configuration
            ("device",  throw_configuration_invalid)
            ["config"];

    try
    {
        return biometry::device_registry().at(id.value().string())->create(config);
    } catch(...) { std::throw_with_nested(biometry::cmds::Test::CouldNotInstiantiateDevice{});}
}
}

biometry::cmds::Test::ConfigurationInvalid::ConfigurationInvalid()
    : std::runtime_error{"Configuration is invalid"}
{
}


biometry::cmds::Test::CouldNotInstiantiateDevice::CouldNotInstiantiateDevice()
    : std::runtime_error{"Could not instantiate device"}
{
}

biometry::cmds::Test::Test()
    : CommandWithFlagsAndAction{cli::Name{"test"}, cli::Usage{"executes runtime tests for a device"}, cli::Description{"executes runtime tests for a device"}}
{
    flag(cli::make_flag(cli::Name{"config"}, cli::Description{"configuration file for the test"}, config));
    flag(cli::make_flag(cli::Name{"user"}, cli::Description{"The numeric user id for testing purposes"}, user = biometry::User::current()));
    flag(cli::make_flag(cli::Name{"trials"}, cli::Description{"Number of identification trials"}, trials = 20));

    action([this](const cli::Command::Context& ctxt)
    {
        auto device = config ? device_from_config_file(*config) : biometry::dbus::Service::create_stub()->default_device();

        ctxt.cout
                << "We are about to execute a test run for a biometric device."                         << std::endl
                << "Please note that we are executing the test in a production"                         << std::endl
                << "environment and you should consider the test to be harmful to the"                  << std::endl
                << "device configuration:"                                                              << std::endl
                << "  User:        " << user                                                            << std::endl
                << "  Config:      " << (Test::config ? (*Test::config).string() : "Default device")    << std::endl
                << "Would you really like to proceed (y/n)?";

        static constexpr const char yes{'y'}; char answer; ctxt.cin >> answer;
        return answer == yes ? test_device(user, ctxt, device) : EXIT_FAILURE;

        return EXIT_FAILURE;
    });
}

int biometry::cmds::Test::test_device(const User& user, const cli::Command::Context& ctxt, const std::shared_ptr<Device>& device)
{
    static std::ofstream dev_null{"/dev/null"};

    ctxt.cout << std::endl;
    {
        ctxt.cout << "STP0" << std::endl;
        auto observer = std::make_shared<SyncingObserver<biometry::TemplateStore::Clearance>>(ctxt.cout, "Clearing template store: ", 17);
        ctxt.cout << "STP1" << std::endl;
        device->template_store().clear(biometry::Application::system(), user)->start_with_observer(observer);
        ctxt.cout << "STP2" << std::endl;
        try { observer->sync(); }
        catch(std::exception& e) {
          ctxt.cout << "\n  Exception: " << e.what() << std::endl;
          ctxt.cout << "  Failed to clear template store, proceeding though... " << std::endl;
        };
    }

    {
        ctxt.cout << "STP3" << std::endl;
        auto observer = std::make_shared<SyncingObserver<biometry::TemplateStore::Enrollment>>(ctxt.cout, "Enrolling new template:  ", 17);
        ctxt.cout << "STP4" << std::endl;
        device->template_store().enroll(biometry::Application::system(), user)->start_with_observer(observer);
        ctxt.cout << "STP5" << std::endl;
        try { observer->sync(); }
        catch(std::exception& e) {
          ctxt.cout << "\n  Exception: " << e.what() << std::endl;
          ctxt.cout << "  Failed to enroll template, aborting ..." << std::endl; return EXIT_FAILURE;
        };
    }

    {
        auto observer = std::make_shared<SyncingObserver<biometry::TemplateStore::SizeQuery>>(ctxt.cout, "Querying template count: ", 17);
        device->template_store().size(biometry::Application::system(), user)->start_with_observer(observer);
        try { observer->sync(); }
        catch(std::exception& e) {
          ctxt.cout << "\n  Exception: " << e.what() << std::endl;
          ctxt.cout << "  Failed to query template count, aborting ..." << std::endl; return EXIT_FAILURE;
        };
    }

    biometry::util::cli::ProgressBar pb{ctxt.cout, "Identifying user:        ", 17};

    auto stats = biometry::util::Benchmark{[device, &ctxt]()
    {
        auto observer = std::make_shared<SyncingObserver<biometry::Identification>>(dev_null, "  Trial: ");
        device->identifier().identify_user(biometry::Application::system(), biometry::Reason{"testing"})
            ->start_with_observer(observer);
        try { observer->sync(); }
        catch(std::exception& e) {
          ctxt.cout << "\n  Exception: " << e.what() << std::endl;
          ctxt.cout << "  Failed to identify user." << std::endl;
        };

    }}.trials(25).on_progress([&pb](std::size_t current, std::size_t total) { pb.update(current/static_cast<double>(total)); }).run();

    ctxt.cout << std::endl
              << "    min:      " << std::setw(6) << std::right << std::fixed << std::setprecision(2) << stats.min()                 << " [µs]" << std::endl
              << "    mean:     " << std::setw(6) << std::right << std::fixed << std::setprecision(2) << stats.mean()                << " [µs]" << std::endl
              << "    std.dev.: " << std::setw(6) << std::right << std::fixed << std::setprecision(2) << std::sqrt(stats.variance()) << " [µs]" << std::endl
              << "    max:      " << std::setw(6) << std::right << std::fixed << std::setprecision(2) << stats.max()                 << " [µs]" << std::endl;

    return EXIT_SUCCESS;
}
