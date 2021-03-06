#include "ns_as5048b.h"
#include "esp32/rom/ets_sys.h"

// Constructor(s)
RPS::RPS() {}                                                           // constructor

// Sensor initialization
void RPS::init(sensorConfig sensorConfig) {
    i2c_config_t conf;                                                  // I2C config data
    conf.mode = I2C_MODE_MASTER;                                        // set Master mode
    conf.sda_io_num = sensorConfig.i2c_gpio_sda;                        // data pin
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;                            // pullup enable (should not harm is HW pullup is present)
    conf.scl_io_num = sensorConfig.i2c_gpio_scl;                        // clock pin
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;                            // pullup enable (should not harm is HW pullup is present)
    conf.master.clk_speed = sensorConfig.i2c_frequency;                 // set clock speed
    conf.clk_flags = 0;
    printf("sda: %d, scl: %d\n", conf.sda_io_num, conf.scl_io_num);
    esp_err_t test = i2c_param_config(sensorConfig.i2c_port, &conf);                     // set config
    printf("test: %d\n", test);
    i2c_driver_install(sensorConfig.i2c_port, I2C_MODE_MASTER, 0, 0, 0);// install
    _chipAddress = sensorConfig.chipAddress;                            // save chip address
    _i2c_port = sensorConfig.i2c_port;                                  // and port
    _inverted = false;                                                  // initial setting non inverted values
    if (sensorConfig.rpsFrontMount) {                                   // if sensor mounted at front, readings are equal to observer (looking from the front) 
        _clockwise = true;                                              // normal CW-incremental sensor readings when motor spins CW (as observed from front)
    } else {                                                            // if sensor is placed at the back, sensor readings should be inverted
        _clockwise = false;                                             // signal that sensor is running backward and should be inverted to also give CW-incremental values when motor spins CW
    }
}

// Get data-high
int RPS::getDataH() {return _data_h;}                                   // return data-high (when needed for debugging)

// Get data-low
int RPS::getDataL() {return _data_l;}                                   // return data-low (when needed for debugging)

// Invert readings
// void RPS::invertReadings() {_inverted = true;}                          // invert readings, readings are incremental the other way around
void RPS::invertReadings() {_inverted = !_inverted;}                          // invert readings, readings are incremental the other way around

// Get Inverted state
bool RPS::isInverted() {return _inverted;}                              // get current inverted state

// Get angle RAW
int RPS::getAngleR() {
    int ret = readAngle();                                              // read RAW angle value
    if (ret == ESP_OK) {                                                // read succeeded?
        uint16_t angleVal = _data_h << 6;                               // shift high bits
        angleVal += (_data_l & 0x3F);                                   // add low bits
        if (_clockwise) {                                                   // if sensor mounted at front-side, normal readings (CW-incremental)
            return _inverted ? AS5048B_RESOLUTION - angleVal : angleVal;    // but if inverted by request (for positive angles on CCW spin), invert readings
        } else {                                                            // if sensor mounted at back-side, inverted readings (CCW-incremental)
            return _inverted ? angleVal : AS5048B_RESOLUTION - angleVal;    // but if inverted by request (for positive angles on CCW spin), invert inverted readings (=normal readings again)
        }
    } else {
        return ret > 0 ? -1 * ret : ret;                                // or return error (negative value)
    }
}

// Reset angle
int RPS::resetAngleZero() {
    int ret = writeZeroAngle(0);                                        // write zero value
    if (ret == ESP_OK) {                                                // write succeeded?
        ret = readAngle();                                              // read raw angle
        if (ret == ESP_OK) {                                            // read succeeded?
            uint16_t angleVal = _data_h << 6;                           // shift high bits
            angleVal += (_data_l & 0x3F);                               // add low bits
            ret = writeZeroAngle(angleVal);                             // write back angle
            if (ret == ESP_OK) {                                        // write succeeded?
                return 0;                                               // all done okay
            } else {
                return ret > 0 ? -1 * ret : ret;                        // return negative error value
            }
        } else {
            return ret > 0 ? -1 * ret : ret;                            // return negative error value
        }
    } else {
        return ret > 0 ? -1 * ret : ret;                                // return negative error value
    }
}

// read RAW angle value
int RPS::readAngle() {
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();                       // create I2C command handle
    i2c_master_start(cmd);                                              // start
    i2c_master_write_byte(cmd, _chipAddress << 1 | WRITE_BIT, ACK_CHECK_EN);    // write chip address with write bit
    i2c_master_write_byte(cmd, ANGLE_REG, ACK_CHECK_EN);                // write register
    i2c_master_stop(cmd);                                               // stop
    ret = i2c_master_cmd_begin(_i2c_port, cmd, 10);                     // send queued commands, wait 10 ticks
    i2c_cmd_link_delete(cmd);                                           // delete link
    if (ret != ESP_OK) {                                                // NOT succeeded?
        return ret;                                                     // return
    }
    cmd = i2c_cmd_link_create();                                        // create I2C command handle
    i2c_master_start(cmd);                                              // start
    i2c_master_write_byte(cmd, _chipAddress << 1 | READ_BIT, ACK_CHECK_EN);     // write chip address with read bit
    i2c_master_read_byte(cmd, &_data_l, I2C_MASTER_ACK);                // write data-low address
    i2c_master_read_byte(cmd, &_data_h, I2C_MASTER_NACK);               // write data-high address
    i2c_master_stop(cmd);                                               // stop
    ret = i2c_master_cmd_begin(_i2c_port, cmd, 10);                     // send queued commands, wait 10 ticks
    i2c_cmd_link_delete(cmd);                                           // delete link
    return ret;                                                         // return success/fail
}

// Set zero angle
int RPS::writeZeroAngle(uint16_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();                       // create I2C command handle
    i2c_master_start(cmd);                                              // start
    i2c_master_write_byte(cmd, _chipAddress << 1 | WRITE_BIT, ACK_CHECK_EN);    // write chip address with write bit
    i2c_master_write_byte(cmd, ZERO_REG, ACK_CHECK_EN);                 // write zero register
    i2c_master_write_byte(cmd, (uint8_t) (value >> 6), ACK_CHECK_EN);   // write data-high
    i2c_master_write_byte(cmd, (uint8_t) (value & 0x3F), ACK_CHECK_EN); // write data-low
    i2c_master_stop(cmd);                                               // stop
    esp_err_t ret = i2c_master_cmd_begin(_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);  // send queued commands, wait 1 sec max
    i2c_cmd_link_delete(cmd);                                           // delete link
    return ret;                                                         // return success/fail
}
