//
//  VoodooI2CFTETouchpadDriver.hpp
//  VoodooI2CFTE
//
//  Created by Kishor Prins on 2017-10-13.
//  Copyright © 2017 Kishor Prins. All rights reserved.
//

#ifndef VOODOOI2C_FTE_TOUCHPAD_DRIVER_HPP
#define VOODOOI2C_FTE_TOUCHPAD_DRIVER_HPP

#include <IOKit/IOService.h>

#include "../../../VoodooI2C/VoodooI2C/VoodooI2CDevice/VoodooI2CDeviceNub.hpp"

#include "../../../Multitouch Support/VoodooI2CMultitouchInterface.hpp"
#include "../../../Multitouch Support/MultitouchHelpers.hpp"

#include "../../../Dependencies/helpers.hpp"

#define FTE_NAME "FTE"

// Message types defined by ApplePS2Keyboard
enum
{
    // from keyboard to mouse/touchpad
    kKeyboardSetTouchStatus = iokit_vendor_specific_msg(100),   // set disable/enable touchpad (data is bool*)
    kKeyboardGetTouchStatus = iokit_vendor_specific_msg(101),   // get disable/enable touchpad (data is bool*)
    kKeyboardKeyPressTime = iokit_vendor_specific_msg(110)      // notify of timestamp a non-modifier key was pressed (data is uint64_t*)
};
/*
void delay(int c) {
    for (int i=c; i>0; i--) {
        int a = 1+1;
    }
}

void delay2(int c) {
    for (int i=c; i>0; i--) {
        delay(32000);
    }
}*/

/* Main class that handles all communication between macOS, VoodooI2C, and a I2C based FTE touchpad */

class VoodooI2CFTETouchpadDriver : public IOService {
    OSDeclareDefaultStructors(VoodooI2CFTETouchpadDriver);
    
    VoodooI2CDeviceNub* api;
    IOACPIPlatformDevice* acpi_device;

 public:
    /* Initialises the VoodooI2CFTETouchpadDriver object/instance (intended as IOKit driver ctor)
     *
     * @return true if properly initialised
     */
    bool init(OSDictionary* properties) override;
    /* Frees any allocated resources, called implicitly by the kernel
     * as the last stage of the driver being unloaded
     *
     */
    void free() override;
    /* Checks if an FTE device exists on the current system
     *
     * @return returns an instance of the current VoodooI2CFTETouchpadDriver if there is a matched FTE device, NULL otherwise
     */
    VoodooI2CFTETouchpadDriver* probe(IOService* provider, SInt32* score) override;
    /* Starts the driver and initialises the FTE device
     *
     * @return returns true if the driver has started
     */
    bool start(IOService* provider) override;
    /* Stops the driver and frees any allocated resource
     *
     */
    void stop(IOService* device) override;
    
protected:
    IOReturn setPowerState(unsigned long longpowerStateOrdinal, IOService* whatDevice) override;

private:
    bool awake;
    bool read_in_progress;
    bool ready_for_input;
    
    char device_name[10];
    char FTE_name[5];
    
    int pressure_adjustment;
    int product_id;
    
    IOCommandGate* command_gate;
    IOInterruptEventSource* interrupt_source;
    VoodooI2CMultitouchInterface *mt_interface;
    OSArray* transducers;
    IOWorkLoop* workLoop;
    
    bool ignoreall;
    uint64_t maxaftertyping = 500000000;
    uint64_t keytime = 0;
    
    /* Handles any interrupts that the FTE device generates
     * @productId product ID of the FTE device
     * @ic_type IC type (provided by the device)
     *
     * @return returns true if this FTE device is ASUS manufactured
     */
    bool check_ASUS_firmware(UInt8 productId, UInt8 ic_type);
    
    
    bool asus_start_multitach();
    
    
    /* Handles input in a threaded manner, then
     * calls parse_FTE_report via the command gate for synchronisation
     *
     */
    void handle_input_threaded();
    /* Sends the appropriate FTE protocol packets to
     * initialise the device into multitouch mode
     *
     * @return true if the device was initialised properly
     */
    bool init_device();
    /* Handles any interrupts that the FTE device generates
     * by spawning a thread that is out of the inerrupt context
     *
     */
    void interrupt_occurred(OSObject* owner, IOInterruptEventSource* src, int intCount);
    /* Reads the FTE report (touch data) in the I2C bus and generates a VoodooI2C multitouch event
     *
     * @return returns a IOReturn status of the reads (usually a representation of I2C bus)
     */
    IOReturn parse_FTE_report();
    /* Initialises the VoodooI2C multitouch classes
     *
     * @return true if the VoodooI2C multitouch classes were properly initialised
     */
    bool publish_multitouch_interface();
    /* Reads a FTE command from the I2C bus
     * @reg which register to read the data from
     * @val a buffer which is large enough to hold the FTE command data
     *
     * @return returns a IOReturn status of the reads (usually a representation of I2C bus)
     */
    IOReturn read_FTE_cmd(UInt16 reg, UInt8* val);
    /* Reads raw data from the I2C bus
     * @reg which 16bit register to read the data from
     * @len the length of the @val buffer
     * @vaue a buffer which is large enough to hold the data being read
     *
     * @return returns a IOReturn status of the reads (usually a representation of I2C bus)
     */
    IOReturn read_raw_16bit_data(UInt16 reg, size_t len, UInt8* values);
    /* Reads raw data from the I2C bus
     * @reg which 8bit register to read the data from
     * @len the length of the @val buffer
     * @vaue a buffer which is large enough to hold the data being read
     *
     * @return returns a IOReturn status of the reads (usually a representation of I2C bus)
     */
    IOReturn read_raw_data(UInt8 reg, size_t len, UInt8* values);
    /* Releases any allocated resources (called by stop)
     *
     */
    void release_resources();
    /* Releases any allocated resources
     *
     * @return true if the FTE device was reset succesfully
     */
    bool reset_device();
    /* Enables or disables the FTE device for sleep
     *
     */
    void set_sleep_status(bool enable);
    /* Releases any allocated VoodooI2C multitouch device
     *
     */
    void unpublish_multitouch_interface();
    /* Writes a FTE command formatted I2C message
     * @reg which register to write the data to
     * @cmd the command which we want to write
     *
     * @return returns a IOReturn status of the reads (usually a representation of I2C bus)
     */
    IOReturn write_FTE_cmd(UInt16 reg, UInt16 cmd);
    
    /*
     * Called by ApplePS2Controller to notify of keyboard interactions
     * @type Custom message type in iokit_vendor_specific_msg range
     * @provider Calling IOService
     * @argument Optional argument as defined by message type
     *
     * @return kIOSuccess if the message is processed
     */
    virtual IOReturn message(UInt32 type, IOService* provider, void* argument);
};

#endif /* VOODOOI2C_FTE_TOUCHPAD_DRIVER_HPP */
