#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

/*=========================================================================
    I2C ADDRESS/BITS
    -----------------------------------------------------------------------*/
#define ADXL375_ADDRESS (0x53) /**< Assumes ALT address pin low */
/*=========================================================================*/

/*=========================================================================
    REGISTERS
    -----------------------------------------------------------------------*/
#define ADXL375_REG_DEVID (0x00)          /**< Device ID */
#define ADXL375_REG_THRESH_TAP (0x1D)     /**< Tap threshold */
#define ADXL375_REG_OFSX (0x1E)           /**< X-axis offset */
#define ADXL375_REG_OFSY (0x1F)           /**< Y-axis offset */
#define ADXL375_REG_OFSZ (0x20)           /**< Z-axis offset */
#define ADXL375_REG_DUR (0x21)            /**< Tap duration */
#define ADXL375_REG_LATENT (0x22)         /**< Tap latency */
#define ADXL375_REG_WINDOW (0x23)         /**< Tap window */
#define ADXL375_REG_THRESH_ACT (0x24)     /**< Activity threshold */
#define ADXL375_REG_THRESH_INACT (0x25)   /**< Inactivity threshold */
#define ADXL375_REG_TIME_INACT (0x26)     /**< Inactivity time */
#define ADXL375_REG_ACT_INACT_CTL (0x27)  /**< Axis enable control for activity and inactivity detection */
#define ADXL375_REG_THRESH_FF (0x28)      /**< Free-fall threshold */
#define ADXL375_REG_TIME_FF (0x29)        /**< Free-fall time */
#define ADXL375_REG_TAP_AXES (0x2A)       /**< Axis control for single tap/double tap */
#define ADXL375_REG_ACT_TAP_STATUS (0x2B) /**< Source of single tap/double tap */
#define ADXL375_REG_BW_RATE (0x2C)        /**< Data rate and power mode control */
#define ADXL375_REG_POWER_CTL (0x2D)      /**< Power-saving features control */
#define ADXL375_REG_INT_ENABLE (0x2E)     /**< Interrupt enable control */
#define ADXL375_REG_INT_MAP (0x2F)        /**< Interrupt mapping control */
#define ADXL375_REG_INT_SOURCE (0x30)     /**< Source of interrupts */
#define ADXL375_REG_DATA_FORMAT (0x31)    /**< Data format control */
#define ADXL375_REG_DATAX0 (0x32)         /**< X-Axis Data 0 */
#define ADXL375_REG_DATAX1 (0x33)         /**< X-Axis Data 1 */
#define ADXL375_REG_DATAY0 (0x34)         /**< Y-Axis Data 0 */
#define ADXL375_REG_DATAY1 (0x35)         /**< Y-Axis Data 1 */
#define ADXL375_REG_DATAZ0 (0x36)         /**< Z-Axis Data 0 */
#define ADXL375_REG_DATAZ1 (0x37)         /**< Z-Axis Data 1 */
#define ADXL375_REG_FIFO_CTL (0x38)       /**< FIFO control */
#define ADXL375_REG_FIFO_STATUS (0x39)    /**< FIFO status */

/*=========================================================================
    CONSTANTS
    -----------------------------------------------------------------------*/
#define ADXL375_MG2G_MULTIPLIER (0.049f) /**< 49mg per lsb */
#define ADXL375_GRAVITY_EARTH (9.80665f) /**< Earth's gravity in m/s^2 */

class ADXL375 {
public:
    /**
     * @brief I2C Constructor
     * @param sensorID Optional ID for the sensor
     */
    ADXL375(int32_t sensorID = -1);

    /**
     * @brief Hardware SPI Constructor
     * @param cs Chip Select pin
     * @param theSPI SPI Class Object
     * @param sensorID Optional ID
     */
    ADXL375(uint8_t cs, SPIClass *theSPI, int32_t sensorID = -1);

    /**
     * @brief Software SPI Constructor
     * @param clock SCK pin
     * @param miso MISO pin
     * @param mosi MOSI pin
     * @param cs Chip Select pin
     * @param sensorID Optional ID
     */
    ADXL375(uint8_t clock, uint8_t miso, uint8_t mosi, uint8_t cs, int32_t sensorID = -1);

    /**
     * @brief Initialize the sensor
     * @param i2caddr I2C address (default 0x53)
     * @return true if successful
     */
    bool begin(uint8_t i2caddr = ADXL375_ADDRESS);

    /**
     * @brief Read acceleration data in m/s^2
     * @param x Pointer to store X acceleration
     * @param y Pointer to store Y acceleration
     * @param z Pointer to store Z acceleration
     */
    void getAcceleration(float* x, float* y, float* z);

    // Raw data access if needed
    void getXYZ(int16_t* x, int16_t* y, int16_t* z);

    int16_t getX();
    int16_t getY();
    int16_t getZ();

    /**
     * @brief Set Trim Offsets
     * @param x Offset for X axis
     * @param y Offset for Y axis
     * @param z Offset for Z axis
     */
    void setTrimOffsets(int8_t x, int8_t y, int8_t z);

    /**
     * @brief Get Trim Offsets
     * @param x Pointer to store X offset
     * @param y Pointer to store Y offset
     * @param z Pointer to store Z offset
     */
    void getTrimOffsets(int8_t* x, int8_t* y, int8_t* z);

private:
    int32_t _sensorID;
    
    // Communication variables
    TwoWire *_wire;
    SPIClass *_spi;
    uint8_t _i2caddr;
    uint8_t _cs, _clk, _mosi, _miso;
    bool _i2c;

    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    int16_t read16(uint8_t reg);
    void readLen(uint8_t reg, uint8_t *buffer, uint8_t len);

    // Low level SPI helper
    uint8_t spixfer(uint8_t x);
};
