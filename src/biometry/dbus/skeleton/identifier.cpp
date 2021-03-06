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

#include <biometry/dbus/skeleton/identifier.h>

#include <biometry/reason.h>
#include <biometry/dbus/codec.h>
#include <biometry/dbus/interface.h>
#include <biometry/dbus/skeleton/operation.h>

#include <biometry/util/atomic_counter.h>

#include <boost/format.hpp>

namespace
{
core::dbus::Message::Ptr not_permitted_in_reply_to(const core::dbus::Message::Ptr& msg)
{
    return core::dbus::Message::make_error(msg, biometry::dbus::interface::Errors::NotPermitted::name(), "");
}
}

bool biometry::dbus::skeleton::Identifier::RequestVerifier::verify_identify_user_request(const biometry::Application& app, const Credentials& provided)
{
    // Requests for the same app are fine.
    if (app == provided.app)
        return true;

    // If app ids do not match: we unconditionally trust unconfined apps, and no one else.
    return provided.app.as_string() == "unconfined";
}

/// @brief create_for_bus returns a new skeleton::Identifier instance connected to bus, forwarding calls to impl.
biometry::dbus::skeleton::Identifier::Ptr biometry::dbus::skeleton::Identifier::create_for_service_and_object(
        const core::dbus::Bus::Ptr& bus,
        const core::dbus::Service::Ptr& service,
        const core::dbus::Object::Ptr& object,
        const std::reference_wrapper<biometry::Identifier>& impl,
        const std::shared_ptr<RequestVerifier>& request_verifier,
        const std::shared_ptr<CredentialsResolver>& credentials_resolver)
{
    return Ptr{new Identifier{bus, service, object, impl, request_verifier, credentials_resolver}};
}

// From biometry::Identifier.
biometry::Operation<biometry::Identification>::Ptr biometry::dbus::skeleton::Identifier::identify_user(const Application& app, const Reason& reason)
{
    return impl.get().identify_user(app, reason);
}

biometry::dbus::skeleton::Identifier::Identifier(
        const core::dbus::Bus::Ptr& bus,
        const core::dbus::Service::Ptr& service,
        const core::dbus::Object::Ptr& object,
        const std::reference_wrapper<biometry::Identifier>& impl,
        const std::shared_ptr<RequestVerifier>& request_verifier,
        const std::shared_ptr<CredentialsResolver>& credentials_resolver)
    : impl{impl},
      request_verifier{request_verifier},
      credentials_resolver{credentials_resolver},
      bus{bus},
      service{service},
      object{object}
{
    object->install_method_handler<biometry::dbus::interface::Identifier::Methods::IdentifyUser>([this](const core::dbus::Message::Ptr& msg)
    {
        Identifier::credentials_resolver->resolve_credentials(msg, [this, msg](const Optional<RequestVerifier::Credentials>& credentials)
        {
            if (not credentials)
            {
                this->bus->send(not_permitted_in_reply_to(msg));
                return;
            }

            biometry::Application app = biometry::Application::system(); biometry::Reason reason = biometry::Reason::unknown();
            auto reader = msg->reader(); reader >> app >> reason;

            if (not this->request_verifier->verify_identify_user_request(app, credentials.get()))
            {
                this->bus->send(not_permitted_in_reply_to(msg));
                return;
            }

            auto op = identify_user(app, reason);

            core::dbus::types::ObjectPath op_path
            {
                (boost::format{"%1%/operation/identification/%2%/%3%/%4%"}
                    % this->object->path().as_string()
                    % credentials.get().app.as_string()
                    % credentials.get().user.id
                    % util::counter<Identifier>().increment()).str()
            };

            ops.synchronized([this, op_path, op](IdentificationOps::ValueType& ops)
            {
                ops[op_path] = skeleton::Operation<Identification>::create_for_object(this->bus, this->service->add_object_for_path(op_path), op);
            });

            auto reply = core::dbus::Message::make_method_return(msg);
            reply->writer() << op_path;
            this->bus->send(reply);
        });
    });
}

biometry::dbus::skeleton::Identifier::~Identifier()
{
    object->uninstall_method_handler<biometry::dbus::interface::Identifier::Methods::IdentifyUser>();
}
