//
//  VoodooI2CFTETouchpadDriver.cpp
//  VoodooI2CFTE
//
//  Created by Sarvar Khasanov on 2018-08-01.
//  Copyright © 2017 Sarvar Khasanov. All rights reserved.
//

#include "../../../VoodooI2C/VoodooI2C/VoodooI2CController/VoodooI2CControllerDriver.hpp"

#include "VoodooI2CFTETouchpadDriver.hpp"
#include "VoodooI2CFTEConstants.h"

#define super IOService
OSDefineMetaClassAndStructors(VoodooI2CFTETouchpadDriver, IOService);

void VoodooI2CFTETouchpadDriver::free() {
    IOLog("%s::%s VoodooI2CFTE resources have been deallocated\n", getName(), FTE_name);
    super::free();
}

void VoodooI2CFTETouchpadDriver::handleInputThreaded() {
    if (!ready_for_input) {
        return;
    }
    command_gate->attemptAction(OSMemberFunctionCast(IOCommandGate::Action, this, &VoodooI2CFTETouchpadDriver::parseFTEReport));
    read_in_progress = false;
}

bool VoodooI2CFTETouchpadDriver::init(OSDictionary *properties) {
    transducers = NULL;
    if (!super::init(properties)) {
        return false;
    }
    transducers = OSArray::withCapacity(ETP_MAX_FINGERS);
    if (!transducers) {
        return false;
    }
    DigitiserTransducerType type = kDigitiserTransducerFinger;
    for (int i = 0; i < ETP_MAX_FINGERS; i++) {
        VoodooI2CDigitiserTransducer* transducer = VoodooI2CDigitiserTransducer::transducer(type, NULL);
        transducers->setObject(transducer);
    }
    awake = true;
    ready_for_input = false;
    read_in_progress = false;
    strncpy(FTE_name, FTE_NAME, strlen(FTE_NAME));
    return true;
}

bool VoodooI2CFTETouchpadDriver::initDevice() {
    if (!resetDevice()) {
        return false;
    }

    // IOReturn ret_val;
    // UInt8 val[3];

    UInt32 max_report_x = 0;
    UInt32 max_report_y = 0;

    /*retVal = readFTECmd(ETP_I2C_FW_VERSION_CMD, val);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get version cmd\n", getName(), device_name);
        return false;
    }*/

    UInt8 version = 0;  // val[0];
    /*retVal = readFTECmd(ETP_I2C_FW_CHECKSUM_CMD, val);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get checksum cmd\n", getName(), device_name);
        return false;
    }*/
    UInt16 csum = 0;  // *reinterpret_cast<UInt16 *>(val);
    /*retVal = readFTECmd(ETP_I2C_IAP_VERSION_CMD, val);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get IAP version cmd\n", getName(), device_name);
        return false;
    }*/
    UInt8 iapversion = 0;  // val[0];
    /*retVal = read_FTE_cmd(ETP_I2C_PRESSURE_CMD, val);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get pressure cmd\n", getName(), device_name);
        return false;
    }
    if ((val[0] >> 4) & 0x1)
        pressure_adjustment = 0;
    else
        pressure_adjustment = ETP_PRESSURE_OFFSET;*/
    /*retVal = readFTECmd(ETP_I2C_MAX_X_AXIS_CMD, val);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get max X axis cmd\n", getName(), device_name);
        return false;
    }*/
    max_report_x = 0x0aea;  // (*(reinterpret_cast<UInt16 *>(val))) & 0x0fff;

    /*retVal = read_FTE_cmd(ETP_I2C_MAX_Y_AXIS_CMD, val);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get max Y axis cmd\n", getName(), device_name);
        return false;
    }*/
    max_report_y = 0x06de;  // (*(reinterpret_cast<UInt16 *>(val))) & 0x0fff;

    /*retVal = read_FTE_cmd(ETP_I2C_XY_TRACENUM_CMD, val);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get XY tracenum cmd\n", getName(), device_name);
        return false;
    }
    retVal = read_FTE_cmd(ETP_I2C_RESOLUTION_CMD, val);
    if (retVal != kIOReturnSuccess) {
        
        return false;
    }*/
    UInt32 hw_res_x = 1;  // val[0];
    UInt32 hw_res_y = 1;  // val[1];

    hw_res_x = (hw_res_x * 10 + 790) * 10 / 254;
    hw_res_y = (hw_res_y * 10 + 790) * 10 / 254;

    IOLog("%s::%s ProdID: %d Vers: %d Csum: %d IAPVers: %d Max X: %d Max Y: %d\n", getName(), device_name, product_id, version, csum, iapversion, max_report_x, max_report_y);
    if (mt_interface) {
        mt_interface->physical_max_x = max_report_x;  // max_report_x * 10 / hw_res_x;
        mt_interface->physical_max_y = max_report_y;  // max_report_y * 10 / hw_res_y;
        mt_interface->logical_max_x = max_report_x;
        mt_interface->logical_max_y = max_report_y;
    }
    return true;
}

void VoodooI2CFTETouchpadDriver::interruptOccurred(OSObject* owner, IOInterruptEventSource* src, int intCount) {
    if (read_in_progress)
        return;
    if (!awake)
        return;
    read_in_progress = true;
    thread_t new_thread;
    kern_return_t ret = kernel_thread_start(OSMemberFunctionCast(thread_continue_t, this, &VoodooI2CFTETouchpadDriver::handleInputThreaded), this, &new_thread);
    if (ret != KERN_SUCCESS) {
        read_in_progress = false;
        IOLog("%s::%s Thread error while attemping to get input report\n", getName(), device_name);
    } else {
        thread_deallocate(new_thread);
    }
}

IOReturn VoodooI2CFTETouchpadDriver::parseFTEReport() {
    if (!api) {
        IOLog("%s::%s API is null\n", getName(), device_name);
        return kIOReturnError;
    }

    UInt8 report_data[ETP_MAX_REPORT_LEN];
    for (int i = 0; i < ETP_MAX_REPORT_LEN; i++) {
        report_data[i] = 0;
    }
    IOReturn ret_val = readRawData(0, sizeof(report_data), report_data);
    if (ret_val != kIOReturnSuccess) {
        IOLog("%s::%s Failed to handle input\n", getName(), device_name);
        return ret_val;
    }
    if (!transducers) {
        return kIOReturnBadArgument;
    }
    if (report_data[ETP_REPORT_ID_OFFSET] != ETP_REPORT_ID) {
        IOLog("%s::%s Invalid report (%d)\n", getName(), device_name, report_data[ETP_REPORT_ID_OFFSET]);
        return kIOReturnError;
    }

    // Check if input is disabled via ApplePS2Keyboard request
    if (ignoreall)
        return kIOReturnSuccess;

    // Ignore input for specified time after keyboard usage
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);
    uint64_t timestamp_ns;
    absolutetime_to_nanoseconds(timestamp, &timestamp_ns);

    if (timestamp_ns - keytime < maxaftertyping)
        return kIOReturnSuccess;

    UInt8* finger_data = &report_data[ETP_FINGER_DATA_OFFSET];
    UInt8 tp_info = report_data[ETP_TOUCH_INFO_OFFSET];

    int num_fingers = 0;
    for (int i = 0; i < ETP_MAX_FINGERS; i++) {
        VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer,  transducers->getObject(i));
        transducer->type = kDigitiserTransducerFinger;
        if (!transducer) {
            continue;
        }
        bool contact_valid = tp_info & (1U << (3 + i));
        // bool contactValid = tp_info & (0x08 << i);
        bool is_palm = false;
        if (contact_valid) {
            unsigned int touch_major = finger_data[3] >> 4 & 0x07;
            // unsigned int touchPpp = finger_data[3] & 0x0f;

            is_palm = touch_major > 5;
        }
        transducer->is_valid = contact_valid;
        if (contact_valid && !is_palm) {
            unsigned int pos_x = ((finger_data[0] & 0xf0) << 4) | finger_data[1];
            unsigned int pos_y = ((finger_data[0] & 0x0f) << 8) | finger_data[2];

            // unsigned int touchMajor = finger_data[3] >> 4 & 0x07;
            // unsigned int touchPpp = finger_data[3] & 0x0f;
            unsigned int pressure = finger_data[4] & 0x7f;

            // unsigned int pressure = finger_data[4] + pressure_adjustment;
            /*unsigned int mk_x = (finger_data[3] & 0x0f);
            unsigned int mk_y = (finger_data[3] >> 4);*/
            /*unsigned int area_x = mk_x;
            unsigned int area_y = mk_y;*/

            if (mt_interface) {
                transducer->logical_max_x = mt_interface->logical_max_x;
                transducer->logical_max_y = mt_interface->logical_max_y;
                pos_y = transducer->logical_max_y - pos_y - 65535;
                /*area_x = mk_x * (transducer->logical_max_x - ETP_FWIDTH_REDUCE);
                area_y = mk_y * (transducer->logical_max_y - ETP_FWIDTH_REDUCE);*/
            }

            if (pressure > ETP_MAX_PRESSURE) {
                pressure = ETP_MAX_PRESSURE;
            }
            transducer->coordinates.x.update(pos_x, timestamp);
            transducer->coordinates.y.update(pos_y, timestamp);
            transducer->physical_button.update(tp_info & 0x01, timestamp);
            transducer->tip_switch.update(1, timestamp);
            transducer->id = i;
            transducer->secondary_id = i;
            // transducer->pressure_physical_max = ETP_MAX_PRESSURE;
            // transducer->tip_pressure.update(pressure, timestamp);
            num_fingers += 1;
        } else {
            transducer->id = i;
            transducer->secondary_id = i;
            transducer->coordinates.x.update(transducer->coordinates.x.last.value, timestamp);
            transducer->coordinates.y.update(transducer->coordinates.y.last.value, timestamp);
            transducer->physical_button.update(0, timestamp);
            transducer->tip_switch.update(0, timestamp);
            // transducer->pressure_physical_max = ETP_MAX_PRESSURE;
            // transducer->tip_pressure.update(0, timestamp);
        }

        finger_data += ETP_FINGER_DATA_LEN;
    }
    // create new VoodooI2CMultitouchEvent
    VoodooI2CMultitouchEvent event;
    event.contact_count = num_fingers;
    event.transducers = transducers;
    // send the event into the multitouch interface
    if (mt_interface) {
        mt_interface->handleInterruptReport(event, timestamp);
    }
    return kIOReturnSuccess;
}

VoodooI2CFTETouchpadDriver* VoodooI2CFTETouchpadDriver::probe(IOService* provider, SInt32* score) {
    IOLog("%s::%s Touchpad probe\n", getName(), FTE_name);
    if (!super::probe(provider, score)) {
        return NULL;
    }
    acpi_device = OSDynamicCast(IOACPIPlatformDevice, provider->getProperty("acpi-device"));
    if (!acpi_device) {
        IOLog("%s::%s Could not get ACPI device\n", getName(), FTE_name);
        return NULL;
    }
    // check for FTE devices (DSDT must have FTE* defined in the name property)
    OSData* name_data = OSDynamicCast(OSData, provider->getProperty("name"));
    if (!name_data) {
        IOLog("%s::%s Unable to get 'name' property\n", getName(), FTE_name);
        return NULL;
    }
    const char* acpi_name = reinterpret_cast<char*>(const_cast<void*>(name_data->getBytesNoCopy()));
    if (acpi_name[0] != 'F' && acpi_name[1] != 'T'
        && acpi_name[2] != 'E') {
        IOLog("%s::%s FTE device not found, instead found %s\n", getName(), FTE_name, acpi_name);
        return NULL;
    }
    strncpy(device_name, acpi_name, 10);
    IOLog("%s::%s FTE device found (%s)\n", getName(), FTE_name, device_name);
    api = OSDynamicCast(VoodooI2CDeviceNub, provider);
    if (!api) {
        IOLog("%s::%s Could not get VoodooI2C API instance\n", getName(), device_name);
        return NULL;
    }

    return this;
}

bool VoodooI2CFTETouchpadDriver::publishMultitouchInterface() {
    mt_interface = new VoodooI2CMultitouchInterface();
    if (!mt_interface) {
        IOLog("%s::%s No memory to allocate VoodooI2CMultitouchInterface instance\n", getName(), device_name);
        goto multitouch_exit;
    }
    if (!mt_interface->init(NULL)) {
        IOLog("%s::%s Failed to init multitouch interface\n", getName(), device_name);
        goto multitouch_exit;
    }
    if (!mt_interface->attach(this)) {
        IOLog("%s::%s Failed to attach multitouch interface\n", getName(), device_name);
        goto multitouch_exit;
    }
    if (!mt_interface->start(this)) {
        IOLog("%s::%s Failed to start multitouch interface\n", getName(), device_name);
        goto multitouch_exit;
    }
    // Assume we are a touchpad
    mt_interface->setProperty(kIOHIDDisplayIntegratedKey, false);
    // ???? is FTE's Vendor Id
    // 0x04f3 ?
    // 0x0b05 ?
    // 0x2575 ?
    mt_interface->setProperty(kIOHIDVendorIDKey, 0x04f3, 32);
    mt_interface->setProperty(kIOHIDProductIDKey, product_id, 32);
    return true;
multitouch_exit:
    unpublishMultitouchInterface();
    return false;
}

IOReturn VoodooI2CFTETouchpadDriver::readFTECmd(UInt16 reg, UInt8* val) {
    return readRaw16bitData(reg, ETP_I2C_INF_LENGTH, val);
}

IOReturn VoodooI2CFTETouchpadDriver::readRawData(UInt8 reg, size_t len, UInt8* values) {
    IOReturn ret_val = kIOReturnSuccess;
    ret_val = api->writeReadI2C(&reg, 1, values, len);
    return ret_val;
}

IOReturn VoodooI2CFTETouchpadDriver::readRaw16bitData(UInt16 reg, size_t len, UInt8* values) {
    IOReturn ret_val = kIOReturnSuccess;
    UInt16 buffer[] {
        reg
    };
    ret_val = api->writeReadI2C(reinterpret_cast<UInt8*>(&buffer), sizeof(buffer), values, len);
    return ret_val;
}

bool VoodooI2CFTETouchpadDriver::resetDevice() {
    IOReturn ret_val = kIOReturnSuccess;
    UInt8 val[256];
    ret_val = readRaw16bitData(ETP_I2C_DESC_CMD, ETP_I2C_DESC_LENGTH, val);
    if (ret_val != kIOReturnSuccess) {
        IOLog("%s::%s  Failed to get desc cmd\n", getName(), device_name);
        return false;
    }
    ret_val = writeFTECmd(ETP_I2C_STAND_CMD, ETP_I2C_WAKE_UP);
    if (ret_val != kIOReturnSuccess) {
        IOLog("%s::%s Failed to send wake up cmd (workaround)\n", getName(), device_name);
        return false;
    }
    IOSleep(200);
    ret_val = writeFTECmd(ETP_I2C_STAND_CMD, ETP_I2C_RESET);
    if (ret_val != kIOReturnSuccess) {
        IOLog("%s::%s Failed to write RESET cmd\n", getName(), device_name);
        return false;
    }
    IOSleep(100);
    UInt8 val2[3];
    ret_val = readRawData(0x00, ETP_I2C_INF_LENGTH, val);
    if (ret_val != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get reset acknowledgement\n", getName(), device_name);
        return false;;
    }
    /*
    retVal = read_raw_16bit_data(ETP_I2C_REPORT_DESC_CMD, ETP_I2C_REPORT_DESC_LENGTH, val);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get report cmd\n", getName(), device_name);
        return false;
    }*/
    // get the product ID
    ret_val = readFTECmd(ETP_I2C_UNIQUEID_CMD, val2);
    if (ret_val != kIOReturnSuccess) {
        IOLog("%s::%s Failed to get product ID cmd\n", getName(), device_name);
        return false;
    }
    product_id = val2[0];
    asus_start_multitach();

    return true;
}

bool VoodooI2CFTETouchpadDriver::asus_start_multitach() {
    IOLog("%s::%s Start multitach enable\n", getName(), device_name);
    UInt8 buffer[] { 0x05, 0x00, 0x3d, 0x03, 0x06, 0x00, 0x07, 0x00, 0x0d, 0x00, 0x03, 0x01, 0x00};
    IOReturn ret_val = kIOReturnSuccess;
    ret_val = api->writeI2C(reinterpret_cast<UInt8*>(&buffer), sizeof(buffer));
    if (ret_val != kIOReturnSuccess) {
        IOLog("%s::%s Failed multitouch initialize\n", getName(), device_name);
        return false;
    }
    return kIOReturnSuccess;
}

void VoodooI2CFTETouchpadDriver::releaseResources() {
    if (command_gate) {
        work_loop->removeEventSource(command_gate);
        command_gate->release();
        command_gate = NULL;
    }
    if (interrupt_source) {
        interrupt_source->disable();
        work_loop->removeEventSource(interrupt_source);
        interrupt_source->release();
        interrupt_source = NULL;
    }
    if (work_loop) {
        work_loop->release();
        work_loop = NULL;
    }
    if (acpi_device) {
        acpi_device->release();
        acpi_device = NULL;
    }
    if (api) {
        if (api->isOpen(this)) {
            api->close(this);
        }
        api->release();
        api = NULL;
    }
    if (transducers) {
        for (int i = 0; i < transducers->getCount(); i++) {
            OSObject* object = transducers->getObject(i);
            if (object) {
                object->release();
            }
        }
        OSSafeReleaseNULL(transducers);
    }
}

IOReturn VoodooI2CFTETouchpadDriver::setPowerState(unsigned long longpower_state_ordinal, IOService* what_device) {
    if (what_device != this)
        return kIOReturnInvalid;
    if (longpower_state_ordinal == 0) {
        if (awake) {
            awake = false;
            for (;;) {
                if (!read_in_progress) {
                    break;
                }
                IOSleep(10);
            }
            IOLog("%s::%s Going to sleep\n", getName(), device_name);
        }
    } else {
        if (!awake) {
            resetDevice();

            awake = true;
            IOLog("%s::%s Woke up and reset device\n", getName(), device_name);
        }
    }
    return kIOPMAckImplied;
}

bool VoodooI2CFTETouchpadDriver::start(IOService* provider) {
    if (!super::start(provider)) {
        return false;
    }
    // Read QuietTimeAfterTyping configuration value (if available)
    OSNumber* quiet_time_after_typing = OSDynamicCast(OSNumber, getProperty("QuietTimeAfterTyping"));
    if (quiet_time_after_typing != NULL)
        maxaftertyping = quiet_time_after_typing->unsigned64BitValue() * 1000000;  // Convert to nanoseconds

    work_loop = this->getWorkLoop();
    if (!work_loop) {
        IOLog("%s::%s Could not get a IOWorkLoop instance\n", getName(), FTE_name);
        return false;
    }
    work_loop->retain();
    command_gate = IOCommandGate::commandGate(this);
    if (!command_gate || (work_loop->addEventSource(command_gate) != kIOReturnSuccess)) {
        IOLog("%s::%s Could not open command gate\n", getName(), FTE_name);
        goto start_exit;
    }
    acpi_device->retain();
    api->retain();
    if (!api->open(this)) {
        IOLog("%s::%s Could not open API\n", getName(), FTE_name);
        goto start_exit;
    }
    // set interrupts AFTER device is initialised
    interrupt_source = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &VoodooI2CFTETouchpadDriver::interruptOccurred), api, 0);
    if (!interrupt_source) {
        IOLog("%s::%s Could not get interrupt event source\n", getName(), FTE_name);
        goto start_exit;
    }
    publishMultitouchInterface();
    if (!initDevice()) {
        IOLog("%s::%s Failed to init device\n", getName(), FTE_name);
        return NULL;
    }
    work_loop->addEventSource(interrupt_source);
    interrupt_source->enable();
    PMinit();
    api->joinPMtree(this);
    registerPowerDriver(this, VoodooI2CIOPMPowerStates, kVoodooI2CIOPMNumberPowerStates);
    IOSleep(100);
    ready_for_input = true;
    setProperty("VoodooI2CServices Supported", OSBoolean::withBoolean(true));
    IOLog("%s::%s VoodooI2CFTE has started\n", getName(), FTE_name);
    mt_interface->registerService();
    registerService();
    return true;
start_exit:
    releaseResources();
    return false;
}

void VoodooI2CFTETouchpadDriver::stop(IOService* provider) {
    releaseResources();
    unpublishMultitouchInterface();
    PMstop();
    IOLog("%s::%s VoodooI2CFTE has stopped\n", getName(), FTE_name);
    super::stop(provider);
}

void VoodooI2CFTETouchpadDriver::unpublishMultitouchInterface() {
    if (mt_interface) {
        mt_interface->stop(this);
        mt_interface->release();
        mt_interface = NULL;
    }
}

// Linux equivalent of FTE_i2c_write_cmd function
IOReturn VoodooI2CFTETouchpadDriver::writeFTECmd(UInt16 reg, UInt16 cmd) {
    UInt16 buffer[] {
        reg,
        cmd
    };
    IOReturn ret_val = kIOReturnSuccess;
    ret_val = api->writeI2C(reinterpret_cast<UInt8*>(&buffer), sizeof(buffer));
    return ret_val;
}

IOReturn VoodooI2CFTETouchpadDriver::message(UInt32 type, IOService* provider, void* argument) {
    switch (type) {
        case kKeyboardGetTouchStatus:
        {
#if DEBUG
            IOLog("%s::getEnabledStatus = %s\n", getName(), ignoreall ? "false" : "true");
#endif
            bool* p_result = reinterpret_cast<bool*>(argument);
            *p_result = !ignoreall;
            break;
        }
        case kKeyboardSetTouchStatus:
        {
            bool enable = *reinterpret_cast<bool*>(argument);
#if DEBUG
            IOLog("%s::setEnabledStatus = %s\n", getName(), enable ? "true" : "false");
#endif
            // ignoreall is true when trackpad has been disabled
            if (enable == ignoreall) {
                // save state, and update LED
                ignoreall = !enable;
            }
            break;
        }
        case kKeyboardKeyPressTime:
        {
            //  Remember last time key was pressed
            keytime = *reinterpret_cast<uint64_t*>(argument);
#if DEBUG
            IOLog("%s::keyPressed = %llu\n", getName(), keytime);
#endif
            break;
        }
    }

    return kIOReturnSuccess;
}
