/*
 * Copyright (C) 2020 Canonical, Ltd.
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
 * Authored by: Erfan Abdi <erfangplus@gmail.com>
 *2
 */

#include <biometry/devices/android.h>
#include <biometry/util/configuration.h>
#include <biometry/util/not_implemented.h>

#include <biometry/device_registry.h>

std::string IntToStringFingerprintError(int error, int vendorCode){
    switch(error) {
        case ERROR_NO_ERROR: return "ERROR_NO_ERROR";
        case ERROR_HW_UNAVAILABLE: return "ERROR_HW_UNAVAILABLE";
        case ERROR_UNABLE_TO_PROCESS: return "ERROR_UNABLE_TO_PROCESS";
        case ERROR_TIMEOUT: return "ERROR_TIMEOUT";
        case ERROR_NO_SPACE: return "ERROR_NO_SPACE";
        case ERROR_CANCELED: return "ERROR_CANCELED";
        case ERROR_UNABLE_TO_REMOVE: return "ERROR_UNABLE_TO_REMOVE";
        case ERROR_LOCKOUT: return "ERROR_LOCKOUT";
        case ERROR_VENDOR: return "ERROR_VENDOR: " + std::to_string(vendorCode);
        default:
            return "ERROR_NO_ERROR";
    }
}

namespace
{
template<typename T>
class androidOperation : public biometry::Operation<T>
{
public:
    typename biometry::Operation<T>::Observer::Ptr mobserver;
    androidOperation(UHardwareBiometry hybris_fp_instance)
     : hybris_fp_instance{hybris_fp_instance}
    {
    }

    void start_with_observer(const typename biometry::Operation<T>::Observer::Ptr& observer) override
    {
        mobserver = observer;
        observer->on_started();
        UHardwareBiometryParams fp_params;
        
        fp_params.enrollresult_cb = enrollresult_cb;
        fp_params.error_cb = error_cb;
       
        fp_params.context = this;
        u_hardware_biometry_setNotify(hybris_fp_instance, &fp_params);
    }

    void cancel() override
    {
        u_hardware_biometry_cancel(hybris_fp_instance);
    }

private:
    UHardwareBiometry hybris_fp_instance;
    
    static void enrollresult_cb(uint64_t deviceId, uint32_t fingerId, uint32_t groupId, uint32_t remaining, void *context)
    {
        ((androidOperation*)context)->mobserver->on_started();
        
        if (T == androidOperation<biometry::TemplateStore::Enrollment)
            printf("enrollresult_cb() called.\n");
    }
    
    static void error_cb(uint64_t, UHardwareBiometryFingerprintError error, int32_t vendorCode, void *context)
    {
        if (error == 0)
            return;

        ((androidOperation*)context)->mobserver->on_failed(IntToStringFingerprintError(error, vendorCode));
        printf("error_cb() called.\n");
    }
    
};
}

biometry::devices::android::TemplateStore::TemplateStore(UHardwareBiometry hybris_fp_instance)
    : hybris_fp_instance{hybris_fp_instance}
{
}

biometry::Operation<biometry::TemplateStore::SizeQuery>::Ptr biometry::devices::android::TemplateStore::size(const biometry::Application&, const biometry::User&)
{
    //TODO: enumerate and count remaining int
    return std::make_shared<androidOperation<biometry::TemplateStore::SizeQuery>>(hybris_fp_instance);
}

biometry::Operation<biometry::TemplateStore::List>::Ptr biometry::devices::android::TemplateStore::list(const biometry::Application&, const biometry::User&)
{
    //TODO: enumerate
    return std::make_shared<androidOperation<biometry::TemplateStore::List>>(hybris_fp_instance);
}

biometry::Operation<biometry::TemplateStore::Enrollment>::Ptr biometry::devices::android::TemplateStore::enroll(const biometry::Application&, const biometry::User& user)
{
    uint64_t token = u_hardware_biometry_preEnroll(hybris_fp_instance);
    UHardwareBiometryRequestStatus ret = u_hardware_biometry_enroll(hybris_fp_instance, new uint8_t[69], user->id, 1000);
    return std::make_shared<androidOperation<biometry::TemplateStore::Enrollment>>(hybris_fp_instance);
}

biometry::Operation<biometry::TemplateStore::Removal>::Ptr biometry::devices::android::TemplateStore::remove(const biometry::Application&, const biometry::User&, biometry::TemplateStore::TemplateId)
{
    return std::make_shared<androidOperation<biometry::TemplateStore::Removal>>(hybris_fp_instance);
}

biometry::Operation<biometry::TemplateStore::Clearance>::Ptr biometry::devices::android::TemplateStore::clear(const biometry::Application&, const biometry::User&)
{
    return std::make_shared<androidOperation<biometry::TemplateStore::Clearance>>(hybris_fp_instance);
}

biometry::devices::android::Identifier::Identifier(UHardwareBiometry hybris_fp_instance)
    : hybris_fp_instance{hybris_fp_instance}
{
}

biometry::Operation<biometry::Identification>::Ptr biometry::devices::android::Identifier::identify_user(const biometry::Application&, const biometry::Reason&)
{
    return std::make_shared<androidOperation<biometry::Identification>>(hybris_fp_instance);
}

biometry::devices::android::Verifier::Verifier(UHardwareBiometry hybris_fp_instance)
    : hybris_fp_instance{hybris_fp_instance}
{
}

biometry::Operation<biometry::Verification>::Ptr biometry::devices::android::Verifier::verify_user(const Application&, const User&, const Reason&)
{
    return std::make_shared<androidOperation<biometry::Verification>>(hybris_fp_instance);
}

biometry::devices::android::android(UHardwareBiometry hybris_fp_instance)
    : template_store_{hybris_fp_instance},
      identifier_{hybris_fp_instance},
      verifier_{hybris_fp_instance}
{
}

biometry::TemplateStore& biometry::devices::android::template_store()
{
    return template_store_;
}

biometry::Identifier& biometry::devices::android::identifier()
{
    return identifier_;
}

biometry::Verifier& biometry::devices::android::verifier()
{
    return verifier_;
}

namespace
{
struct androidDescriptor : public biometry::Device::Descriptor
{
    std::shared_ptr<biometry::Device> create(const biometry::util::Configuration&) override
    {
        return std::make_shared<biometry::devices::android>(u_hardware_biometry_new());
    }

    std::string name() const override
    {
        return "Android HAL Bridge";
    }

    std::string author() const override
    {
        return "Erfan Abdi (erfangplus@gmail.com)";
    }

    std::string description() const override
    {
        return "android is a biometry::Device implementation for connecting to android fp hal using hybris.";
    }
};
}

biometry::Device::Descriptor::Ptr biometry::devices::android::make_descriptor()
{
    return std::make_shared<androidDescriptor>();
}
