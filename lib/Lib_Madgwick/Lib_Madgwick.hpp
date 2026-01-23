#pragma once
#include <Arduino.h>
#include <math.h>

class Madgwick {
public:
    Madgwick();
    void begin(float sampleFrequency);
    void update(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz);
    void updateIMU(float gx, float gy, float gz, float ax, float ay, float az);
    
    // Get orientation
    float getRoll();
    float getPitch();
    float getYaw();
    
    // Get gravity vector in body frame (g = 1.0)
    void getGravity(float *gx, float *gy, float *gz);
    
    // Get linear acceleration (gravity removed)
    // ax, ay, az: raw acceleration in G
    // lin_ax, lin_ay, lin_az: output linear acceleration in G
    void getLinearAcceleration(float ax, float ay, float az, float *lin_ax, float *lin_ay, float *lin_az);

    float q0, q1, q2, q3; // Quaternion
    float invSampleFreq;
    float beta; // Algorithm gain
    float zeta; // Gyro drift gain
    float w_bx, w_by, w_bz; // Estimated gyro bias error

    void setZeta(float zeta);
};
