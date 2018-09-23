//
//  VoodooI2CFTE
//
//  Created by Sarvar Khasanov on 2018-08-01.
//  Copyright © 2017 Sarvar Khasanov. All rights reserved.
//
//  Licensed under GPL v2





/* FTE found commands */
#define ETP_I2C_RESET 0x0100
#define ETP_I2C_WAKE_UP 0x0800
#define ETP_I2C_SET_CMD   0x0300
#define ETP_I2C_NO_CMD   0x0000

#define ETP_I2C_DESC_CMD  0x0001
#define ETP_I2C_STAND_CMD  0x0005
#define ETP_I2C_DESC_LENGTH  30
#define ETP_I2C_UNIQUEID_CMD  0x0101

#define ETP_MAX_FINGERS  5
#define ETP_MAX_REPORT_LEN 34
#define ETP_REPORT_ID_OFFSET 2
#define ETP_REPORT_ID  0x5D
#define ETP_FINGER_DATA_OFFSET 4
#define ETP_TOUCH_INFO_OFFSET 3
#define ETP_MAX_PRESSURE 127
#define ETP_FINGER_DATA_LEN 5
#define ETP_I2C_INF_LENGTH  2





/*
#define ETP_I2C_FW_VERSION_CMD  0x0102
#define ETP_I2C_FW_CHECKSUM_CMD  0x030F
#define ETP_I2C_MAX_X_AXIS_CMD  0x0106
#define ETP_I2C_MAX_Y_AXIS_CMD  0x0107
*/
