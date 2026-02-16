#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

/**
 * @brief ICM-42688 6-Axis IMU Library
 * Supported for Strawberry Linux module
 */

/*=========================================================================
    I2C ADDRESS/BITS
    -----------------------------------------------------------------------*/
#define ICM42688_ADDR_GND (0x68) /**< AD0 pin low */
#define ICM42688_ADDR_VDD (0x69) /**< AD0 pin high */
/*=========================================================================*/

/*=========================================================================
    REGISTERS (Bank 0)
    -----------------------------------------------------------------------*/
#define ICM42688_REG_DEVICE_CONFIG (0x11)
#define ICM42688_REG_DRIVE_CONFIG (0x13)
#define ICM42688_REG_INT_CONFIG (0x14)
#define ICM42688_REG_FIFO_CONFIG (0x16)
#define ICM42688_REG_TEMP_DATA1 (0x1D)
#define ICM42688_REG_TEMP_DATA0 (0x1E)
#define ICM42688_REG_ACCEL_DATA_X1 (0x1F)
#define ICM42688_REG_ACCEL_DATA_X0 (0x20)
#define ICM42688_REG_ACCEL_DATA_Y1 (0x21)
#define ICM42688_REG_ACCEL_DATA_Y0 (0x22)
#define ICM42688_REG_ACCEL_DATA_Z1 (0x23)
#define ICM42688_REG_ACCEL_DATA_Z0 (0x24)
#define ICM42688_REG_GYRO_DATA_X1 (0x25)
#define ICM42688_REG_GYRO_DATA_X0 (0x26)
#define ICM42688_REG_GYRO_DATA_Y1 (0x27)
#define ICM42688_REG_GYRO_DATA_Y0 (0x28)
#define ICM42688_REG_GYRO_DATA_Z1 (0x29)
#define ICM42688_REG_GYRO_DATA_Z0 (0x2A)
#define ICM42688_REG_TMST_FSYNCH (0x2B)
#define ICM42688_REG_TMST_FSYNCL (0x2C)
#define ICM42688_REG_INT_STATUS (0x2D)
#define ICM42688_REG_FIFO_COUNTH (0x2E)
#define ICM42688_REG_FIFO_COUNTL (0x2F)
#define ICM42688_REG_FIFO_DATA (0x30)
#define ICM42688_REG_APEX_DATA0 (0x31)
#define ICM42688_REG_APEX_DATA1 (0x32)
#define ICM42688_REG_APEX_DATA2 (0x33)
#define ICM42688_REG_APEX_DATA3 (0x34)
#define ICM42688_REG_WHO_AM_I (0x75)
#define ICM42688_WHO_AM_I_VALUE (0x47)

#define ICM42688_REG_REG_BANK_SEL (0x76)

// Power Management & Config
#define ICM42688_REG_INTF_CONFIG0 (0x4C)
#define ICM42688_REG_INTF_CONFIG1 (0x4D)
#define ICM42688_REG_PWR_MGMT0 (0x4E)
#define ICM42688_REG_GYRO_CONFIG0 (0x4F)
#define ICM42688_REG_ACCEL_CONFIG0 (0x50)
#define ICM42688_REG_GYRO_CONFIG1 (0x51)
#define ICM42688_REG_GYRO_ACCEL_CONFIG0 (0x52)
#define ICM42688_REG_ACCEL_CONFIG1 (0x53)
#define ICM42688_REG_TMST_CONFIG (0x54)

/*=========================================================================*/

class ICM42688 {
public:
  enum AccelFS {
    ACCEL_FS_16G = 0x00,
    ACCEL_FS_8G = 0x01,
    ACCEL_FS_4G = 0x02,
    ACCEL_FS_2G = 0x03
  };

  enum GyroFS {
    GYRO_FS_2000DPS = 0x00,
    GYRO_FS_1000DPS = 0x01,
    GYRO_FS_500DPS = 0x02,
    GYRO_FS_250DPS = 0x03,
    GYRO_FS_125DPS = 0x04,
    GYRO_FS_62_5DPS = 0x05,
    GYRO_FS_31_25DPS = 0x06,
    GYRO_FS_15_625DPS = 0x07
  };

  enum ODR {
    ODR_32KHZ = 0x01,
    ODR_16KHZ = 0x02,
    ODR_8KHZ = 0x03,
    ODR_4KHZ = 0x04,
    ODR_2KHZ = 0x05,
    ODR_1KHZ = 0x06,
    ODR_200HZ = 0x07,
    ODR_100HZ = 0x08,
    ODR_50HZ = 0x09,
    ODR_25HZ = 0x0A,
    ODR_12_5HZ = 0x0B
  };

  /**
   * @brief I2C Constructor
   */
  ICM42688(TwoWire &wire = Wire);

  ICM42688(uint8_t cs, SPIClass *spi = &SPI);

  /**
   * @brief Software SPI Constructor
   * @param sck Serial Clock pin
   * @param miso Master In Slave Out pin
   * @param mosi Master Out Slave In pin
   * @param cs Chip Select pin
   */
  ICM42688(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs);

  /**
   * @brief Initialize the sensor
   * @param i2caddr I2C address (default 0x68)
   * @return true if successful
   */
  bool begin(uint8_t i2caddr = ICM42688_ADDR_GND);

  /**
   * @brief Read all sensor data
   */
  void readSensor();

  /**
   * @brief Get acceleration in G
   */
  float getAccelX() const { return _ax; }
  float getAccelY() const { return _ay; }
  float getAccelZ() const { return _az; }

  /**
   * @brief Get gyro in deg/s
   */
  float getGyroX() const { return _gx; }
  float getGyroY() const { return _gy; }
  float getGyroZ() const { return _gz; }

  /**
   * @brief Get Temperature in Celsius
   */
  float getTemp() const { return _temp; }

  /**
   * @brief Configuration
   */
  void setAccelFS(AccelFS fs);
  void setGyroFS(GyroFS fs);
  void setAccelODR(ODR odr);
  void setGyroODR(ODR odr);

private:
  TwoWire *_wire = nullptr;
  SPIClass *_spi = nullptr;
  uint8_t _cs = 0xFF;
  uint8_t _sck = 0xFF, _miso = 0xFF, _mosi = 0xFF;
  uint8_t _i2caddr = 0;
  bool _isSPI = false;
  bool _isSoftSPI = false;

  float _ax, _ay, _az;
  float _gx, _gy, _gz;
  float _temp;

  float _accelRes, _gyroRes;
  AccelFS _accelFS = ACCEL_FS_16G;
  GyroFS _gyroFS = GYRO_FS_2000DPS;

  void updateRes();
  void writeRegister(uint8_t reg, uint8_t val);
  uint8_t readRegister(uint8_t reg);
  void readRegisters(uint8_t reg, uint8_t *buffer, uint8_t len);
  uint8_t spixfer(uint8_t x);
};
