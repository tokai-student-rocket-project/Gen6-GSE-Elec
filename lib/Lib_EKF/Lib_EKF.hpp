#pragma once
#include <Arduino.h>
#include <math.h>

class EKF {
public:
    EKF();
    
    // Initialize filter
    // sampleFrequency: Frequency of the update loop in Hz
    void begin(float sampleFrequency);
    
    // Update filter with 9-axis data (Gyro + Accel + Mag)
    // gx, gy, gz: Gyroscope data in rad/s (or deg/s, see implementation notes) -> typically rad/s
    // ax, ay, az: Accelerometer data in m/s^2 or G
    // mx, my, mz: Magnetometer data in uT or arbitrary units (normalized internally)
    void update(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz);
    
    // Update filter with 6-axis data (Gyro + Accel)
    void updateIMU(float gx, float gy, float gz, float ax, float ay, float az);
    
    // Get orientation
    float getRoll();
    float getPitch();
    float getYaw();
    
    // Get gravity vector in body frame (g = 1.0)
    void getGravity(float *gx, float *gy, float *gz);
    
    // Get linear acceleration (gravity removed)
    void getLinearAcceleration(float ax, float ay, float az, float *lin_ax, float *lin_ay, float *lin_az);

    // Get quaternion
    void getQuaternion(float *q0, float *q1, float *q2, float *q3);

    // Helper: Matrix math for EKF (internal use, but maybe exposed for debug)
    // We keep state public for easy debug access if needed, or make private.
    // Making public for now to align with typical library transparency.
    
    float q0, q1, q2, q3; // Quaternion (State 0-3)
    float b_x, b_y, b_z;  // Gyro Bias (State 4-6)
    
    // Covariance Matrix P (7x7)
    // We will store it as a flattened array or simple 2D array
    float P[7][7];
    
    // Tuning parameters
    float Q_angle; // Process noise for angle
    float Q_bias;  // Process noise for bias
    float R_accel; // Measurement noise for accelerometer
    float R_mag;   // Measurement noise for magnetometer

    float invSampleFreq;

private:
    void predict(float gx, float gy, float gz);
    void updateAccel(float ax, float ay, float az);
    void updateMag(float mx, float my, float mz);
};
