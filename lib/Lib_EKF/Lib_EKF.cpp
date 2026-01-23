#include "Lib_EKF.hpp"

// Math constants
#define DEG_TO_RAD 0.01745329251f
#define RAD_TO_DEG 57.2957795131f

// Helper functions for 7x7 and 3x7 matrix math
// Since we don't have a matrix library, we perform specific operations needed.

// Copy 7x7 matrix
void copyMat7x7(float src[7][7], float dst[7][7])
{
    for (int i = 0; i < 7; i++)
        for (int j = 0; j < 7; j++)
            dst[i][j] = src[i][j];
}

// Matrix Multiply: C = A * B. A: 7x7, B: 7x7
void mult7x7(float A[7][7], float B[7][7], float C[7][7])
{
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            C[i][j] = 0.0f;
            for (int k = 0; k < 7; k++)
            {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// Matrix Multiply: C = A * B^T. A: 7x7, B: 7x7
void mult7x7_T(float A[7][7], float B[7][7], float C[7][7])
{
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            C[i][j] = 0.0f;
            for (int k = 0; k < 7; k++)
            {
                C[i][j] += A[i][k] * B[j][k]; // B transposed access
            }
        }
    }
}

EKF::EKF()
{
    q0 = 1.0f;
    q1 = 0.0f;
    q2 = 0.0f;
    q3 = 0.0f;
    b_x = 0.0f;
    b_y = 0.0f;
    b_z = 0.0f;

    invSampleFreq = 1.0f / 100.0f; // Default

    // Initialize P
    for (int i = 0; i < 7; i++)
        for (int j = 0; j < 7; j++)
            P[i][j] = 0.0f;
    // Initial uncertainty
    P[0][0] = P[1][1] = P[2][2] = P[3][3] = 0.001f;
    P[4][4] = P[5][5] = P[6][6] = 0.001f;

    // Tuning parameters (Default values, can be tuned)
    Q_angle = 0.001f;
    Q_bias = 0.003f;
    R_accel = 0.03f;
    R_mag = 0.05f;
}

void EKF::begin(float sampleFrequency)
{
    if (sampleFrequency > 0.0f)
    {
        invSampleFreq = 1.0f / sampleFrequency;
    }
}

void EKF::update(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz)
{
    // 1. Prediction
    predict(gx, gy, gz);

    // 2. Update Accel
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f)))
    {
        updateAccel(ax, ay, az);
    }

    // 3. Update Mag
    if (!((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)))
    {
        updateMag(mx, my, mz);
    }

    // Normalize Quaternion
    float recipNorm = 1.0f / sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;
}

void EKF::updateIMU(float gx, float gy, float gz, float ax, float ay, float az)
{
    predict(gx, gy, gz);
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f)))
    {
        updateAccel(ax, ay, az);
    }

    float recipNorm = 1.0f / sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;
}

void EKF::predict(float gx, float gy, float gz)
{
    // Convert deg/s to rad/s
    gx *= DEG_TO_RAD;
    gy *= DEG_TO_RAD;
    gz *= DEG_TO_RAD;

    // Correct for bias
    float wx = gx - b_x;
    float wy = gy - b_y;
    float wz = gz - b_z;

    float dt = invSampleFreq;
    float dt2 = 0.5f * dt;

    // State Prediction (Quaternion Integration)
    // q_k+1 = q_k + 0.5 * dt * Omega * q_k
    float new_q0 = q0 + dt2 * (-q1 * wx - q2 * wy - q3 * wz);
    float new_q1 = q1 + dt2 * (q0 * wx - q3 * wy + q2 * wz);
    float new_q2 = q2 + dt2 * (q3 * wx + q0 * wy - q1 * wz);
    float new_q3 = q3 + dt2 * (-q2 * wx + q1 * wy + q0 * wz);

    q0 = new_q0;
    q1 = new_q1;
    q2 = new_q2;
    q3 = new_q3;

    // Normalize predicted quaternion (optional but recommended)
    float recipNorm = 1.0f / sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;

    // Transition Matrix F (7x7)
    // F = I + F_cont * dt
    // F_cont for q is 0.5 * Omega
    // F_cont for bias is 0
    // Interaction q and bias: dq/db = -0.5 * dt * Xi(q)

    float F[7][7] = {0};

    // Identity
    for (int i = 0; i < 7; i++)
        F[i][i] = 1.0f;

    // Part dq/dq
    F[0][1] = -dt2 * wx;
    F[0][2] = -dt2 * wy;
    F[0][3] = -dt2 * wz;
    F[1][0] = dt2 * wx;
    F[1][2] = dt2 * wz;
    F[1][3] = -dt2 * wy;
    F[2][0] = dt2 * wy;
    F[2][1] = -dt2 * wz;
    F[2][3] = dt2 * wx;
    F[3][0] = dt2 * wz;
    F[3][1] = dt2 * wy;
    F[3][2] = -dt2 * wx;

    // Part dq/db
    // dq/db = -0.5 * dt * [ -q1 -q2 -q3;
    //                        q0 -q3  q2;
    //                        q3  q0 -q1;
    //                       -q2  q1  q0 ]
    F[0][4] = dt2 * q1;
    F[0][5] = dt2 * q2;
    F[0][6] = dt2 * q3;
    F[1][4] = -dt2 * q0;
    F[1][5] = dt2 * q3;
    F[1][6] = -dt2 * q2;
    F[2][4] = -dt2 * q3;
    F[2][5] = -dt2 * q0;
    F[2][6] = dt2 * q1;
    F[3][4] = dt2 * q2;
    F[3][5] = -dt2 * q1;
    F[3][6] = -dt2 * q0;

    // Propagate Covariance: P = F * P * F' + Q
    // We treat F*P*F' in two steps: Tmp = F*P, then P = Tmp*F'
    float Tmp[7][7];
    mult7x7(F, P, Tmp);
    mult7x7_T(Tmp, F, P);

    // Add Process Noise Q
    P[0][0] += Q_angle;
    P[1][1] += Q_angle;
    P[2][2] += Q_angle;
    P[3][3] += Q_angle;
    P[4][4] += Q_bias;
    P[5][5] += Q_bias;
    P[6][6] += Q_bias;
}

// 3x3 Inverse helper
bool inv3x3(float A[3][3], float Inv[3][3])
{
    float det = A[0][0] * (A[1][1] * A[2][2] - A[2][1] * A[1][2]) -
                A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
                A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);

    if (fabs(det) < 1e-10f)
        return false;

    float invdet = 1.0f / det;

    Inv[0][0] = (A[1][1] * A[2][2] - A[2][1] * A[1][2]) * invdet;
    Inv[0][1] = (A[0][2] * A[2][1] - A[0][1] * A[2][2]) * invdet;
    Inv[0][2] = (A[0][1] * A[1][2] - A[0][2] * A[1][1]) * invdet;
    Inv[1][0] = (A[1][2] * A[2][0] - A[1][0] * A[2][2]) * invdet;
    Inv[1][1] = (A[0][0] * A[2][2] - A[0][2] * A[2][0]) * invdet;
    Inv[1][2] = (A[1][0] * A[0][2] - A[0][0] * A[1][2]) * invdet;
    Inv[2][0] = (A[1][0] * A[2][1] - A[2][0] * A[1][1]) * invdet;
    Inv[2][1] = (A[2][0] * A[0][1] - A[0][0] * A[2][1]) * invdet;
    Inv[2][2] = (A[0][0] * A[1][1] - A[1][0] * A[0][1]) * invdet;
    return true;
}

void EKF::updateAccel(float ax, float ay, float az)
{
    // Normalize Accel
    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm == 0.0f)
        return;
    ax /= norm;
    ay /= norm;
    az /= norm;

    // Measurement h(x): Predicted Gravity Direction in Body Frame
    // g_body = R(q)^T * [0 0 1]^T
    //        = [ 2(q1q3 - q0q2) ]
    //          [ 2(q0q1 + q2q3) ]
    //          [ q0^2 - q1^2 - q2^2 + q3^2 ]

    float hx = 2.0f * (q1 * q3 - q0 * q2);
    float hy = 2.0f * (q0 * q1 + q2 * q3);
    float hz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;

    // Residual y = z - h(x)
    float y[3] = {ax - hx, ay - hy, az - hz};

    // Jacobian H (3x7) = dh/dx
    // dh/dq computed from partial derivatives of hx, hy, hz wrt q
    // dh/db = 0

    float H[3][7] = {0};

    H[0][0] = -2.0f * q2;
    H[0][1] = 2.0f * q3;
    H[0][2] = -2.0f * q0;
    H[0][3] = 2.0f * q1;
    H[1][0] = 2.0f * q1;
    H[1][1] = 2.0f * q0;
    H[1][2] = 2.0f * q3;
    H[1][3] = 2.0f * q2;
    H[2][0] = 2.0f * q0;
    H[2][1] = -2.0f * q1;
    H[2][2] = -2.0f * q2;
    H[2][3] = 2.0f * q3;

    // 4. S = H * P * H' + R
    // Computed sequentially: Tmp = H*P (3x7), S = Tmp*H' (3x3)
    float HP[3][7];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            HP[i][j] = 0.0f;
            for (int k = 0; k < 7; k++)
                HP[i][j] += H[i][k] * P[k][j];
        }
    }

    float S[3][3];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            S[i][j] = 0.0f;
            for (int k = 0; k < 7; k++)
                S[i][j] += HP[i][k] * H[j][k]; // H transpose
        }
    }
    S[0][0] += R_accel;
    S[1][1] += R_accel;
    S[2][2] += R_accel;

    // 5. K = P * H' * S^-1
    // S_inv
    float S_inv[3][3];
    if (!inv3x3(S, S_inv))
        return; // Singularity

    // K = (P * H') * S_inv
    // PHt = P * H' (7x3)
    // K = PHt * S_inv (7x3)
    float PHt[7][3];
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            PHt[i][j] = 0.0f;
            for (int k = 0; k < 7; k++)
                PHt[i][j] += P[i][k] * H[j][k]; // H is real
        }
    }

    float K[7][3];
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            K[i][j] = 0.0f;
            for (int k = 0; k < 3; k++)
                K[i][j] += PHt[i][k] * S_inv[k][j];
        }
    }

    // 6. Update State x = x + K*y
    float dx[7] = {0};
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 3; j++)
            dx[i] += K[i][j] * y[j];
    }

    q0 += dx[0];
    q1 += dx[1];
    q2 += dx[2];
    q3 += dx[3];
    b_x += dx[4];
    b_y += dx[5];
    b_z += dx[6];

    // 7. Update P = (I - K*H) * P
    // P = P - K*H*P = P - K*HP
    float KHP[7][7] = {0};
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            for (int k = 0; k < 3; k++)
                KHP[i][j] += K[i][k] * HP[k][j];
        }
    }

    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            P[i][j] -= KHP[i][j];
        }
    }
}

void EKF::updateMag(float mx, float my, float mz)
{
    float norm = sqrtf(mx * mx + my * my + mz * mz);
    if (norm == 0.0f)
        return;
    mx /= norm;
    my /= norm;
    mz /= norm;

    // Reference Vector in Earth Frame
    // Assume dip angle is ignored for simplicity or estimated?
    // Standard AHRS often estimates reference field direction OR assumes North/Down.
    // Here we compute h = R(q) * m_meas to see where it lands in Earth frame,
    // extract heading correction, or project prediction.

    // Method from Madgwick usually:
    // Rotate measured mag to earth frame: h = R * m
    // b = [sqrt(hx^2 + hy^2), 0, hz] (Dip compensated)
    // Then reference is b.
    // In EKF, usually we assume a fixed reference or adapt it.
    // Let's use the assumption that Earth Mag field has 0 East component.
    // Ref: m_ref = [bN, 0, bD]

    // BUT we need m_ref to compute predicted measurement.
    // Let's first estimate b based on current q and measurement m
    // This is kind of recursive. Alternatively, use fixed reference (normalize to 1).
    // Let's rotate 'm' to Earth frame using current q to find dip.

    // h = R * m_body
    // R = [ ... ]

    float q0q0 = q0 * q0, q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3;
    float h_x = mx * (q0q0 + q1q1 - q2q2 - q3q3) + my * (2.f * (q1 * q2 - q0 * q3)) + mz * (2.f * (q1 * q3 + q0 * q2));
    float h_y = mx * (2.f * (q1 * q2 + q0 * q3)) + my * (q0q0 - q1q1 + q2q2 - q3q3) + mz * (2.f * (q2 * q3 - q0 * q1));
    float h_z = mx * (2.f * (q1 * q3 - q0 * q2)) + my * (2.f * (q2 * q3 + q0 * q1)) + mz * (q0q0 - q1q1 - q2q2 + q3q3);

    float b_norm = sqrtf(h_x * h_x + h_y * h_y);
    // Reference vector in Earth frame (North, East, Down) -> East is 0
    float ref_x = b_norm;
    float ref_y = 0.0f;
    float ref_z = h_z;

    // Predicted Mag in Body Frame
    // m_pred = R^T * ref
    // [ mx ]   [ q0^2+q1^2-q2^2-q3^2    2(q1q2+q0q3)        2(q1q3-q0q2)     ] [ ref_x ]
    // [ my ] = [ 2(q1q2-q0q3)           q0^2-q1^2+q2^2-q3^2 2(q2q3+q0q1)     ] [   0   ]
    // [ mz ]   [ 2(q1q3+q0q2)           2(q2q3-q0q1)        q0^2-q1^2-q2^2+q3^2 ] [ ref_z ]

    float pred_x = ref_x * (q0q0 + q1q1 - q2q2 - q3q3) + ref_z * (2.0f * (q1 * q3 - q0 * q2));
    float pred_y = ref_x * (2.0f * (q1 * q2 - q0 * q3)) + ref_z * (2.0f * (q2 * q3 + q0 * q1));
    float pred_z = ref_x * (2.0f * (q1 * q3 + q0 * q2)) + ref_z * (q0q0 - q1q1 - q2q2 + q3q3);

    // Residual y = z - h(x)
    float y[3] = {mx - pred_x, my - pred_y, mz - pred_z};

    // Jacobian H (3x7)
    // Derivatives of pred_x, pred_y, pred_z w.r.t q0..q3
    // This is complex. We use numerical or analytical Jacobian.
    // Analytical is better.

    float H[3][7] = {0};
    // Helper derivatives: d(R^T * ref)/dq
    // ... Simplified for ref_y = 0

    // d(pred_x)/dq
    H[0][0] = ref_x * 2.0f * q0 + ref_z * -2.0f * q2;
    H[0][1] = ref_x * 2.0f * q1 + ref_z * 2.0f * q3;
    H[0][2] = ref_x * -2.0f * q2 + ref_z * -2.0f * q0;
    H[0][3] = ref_x * -2.0f * q3 + ref_z * 2.0f * q1;

    // d(pred_y)/dq
    H[1][0] = ref_x * -2.0f * q3 + ref_z * 2.0f * q1;
    H[1][1] = ref_x * 2.0f * q2 + ref_z * 2.0f * q0; // typo in derivation?
    // Let's re-derive carefully or assume standard Jacobian
    // pred_y = ref_x(2q1q2 - 2q0q3) + ref_z(2q2q3 + 2q0q1)
    // d/dq0: -2ref_x q3 + 2ref_z q1
    // d/dq1:  2ref_x q2 + 2ref_z q0
    // d/dq2:  2ref_x q1 + 2ref_z q3
    // d/dq3: -2ref_x q0 + 2ref_z q2

    H[1][0] = -2.0f * ref_x * q3 + 2.0f * ref_z * q1;
    H[1][1] = 2.0f * ref_x * q2 + 2.0f * ref_z * q0;
    H[1][2] = 2.0f * ref_x * q1 + 2.0f * ref_z * q3;
    H[1][3] = -2.0f * ref_x * q0 + 2.0f * ref_z * q2;

    // d(pred_z)/dq
    // pred_z = ref_x(2q1q3 + 2q0q2) + ref_z(q0^2 -q1^2 -q2^2 +q3^2)
    H[2][0] = 2.0f * ref_x * q2 + 2.0f * ref_z * q0;
    H[2][1] = 2.0f * ref_x * q3 - 2.0f * ref_z * q1;
    H[2][2] = 2.0f * ref_x * q0 - 2.0f * ref_z * q2;
    H[2][3] = 2.0f * ref_x * q1 + 2.0f * ref_z * q3;

    // Compute S, K, Update similar to Accel
    // 4. S = H * P * H' + R
    float HP[3][7];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            HP[i][j] = 0.0f;
            for (int k = 0; k < 7; k++)
                HP[i][j] += H[i][k] * P[k][j];
        }
    }
    float S[3][3];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            S[i][j] = 0.0f;
            for (int k = 0; k < 7; k++)
                S[i][j] += HP[i][k] * H[j][k];
        }
    }
    S[0][0] += R_mag;
    S[1][1] += R_mag;
    S[2][2] += R_mag;

    float S_inv[3][3];
    if (!inv3x3(S, S_inv))
        return;

    float PHt[7][3];
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            PHt[i][j] = 0.0f;
            for (int k = 0; k < 7; k++)
                PHt[i][k] += P[i][k] * H[j][k];
        }
    }
    float K[7][3];
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            K[i][j] = 0.0f;
            for (int k = 0; k < 3; k++)
                K[i][j] += PHt[i][k] * S_inv[k][j];
        }
    }

    float dx[7] = {0};
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 3; j++)
            dx[i] += K[i][j] * y[j];
    }

    q0 += dx[0];
    q1 += dx[1];
    q2 += dx[2];
    q3 += dx[3];
    b_x += dx[4];
    b_y += dx[5];
    b_z += dx[6];

    float KHP[7][7] = {0};
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            for (int k = 0; k < 3; k++)
                KHP[i][j] += K[i][k] * HP[k][j];
        }
    }
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            P[i][j] -= KHP[i][j]; // Simplest P update: (I - KH)P
            // Note: Joseph form is more robust but more expensive. This is standard EKF.
        }
    }
}

float EKF::getRoll()
{
    return atan2f(q0 * q1 + q2 * q3, 0.5f - q1 * q1 - q2 * q2) * RAD_TO_DEG;
}

float EKF::getPitch()
{
    return asinf(-2.0f * (q1 * q3 - q0 * q2)) * RAD_TO_DEG;
}

float EKF::getYaw()
{
    return atan2f(q1 * q2 + q0 * q3, 0.5f - q2 * q2 - q3 * q3) * RAD_TO_DEG;
}

void EKF::getGravity(float *gx, float *gy, float *gz)
{
    *gx = 2.0f * (q1 * q3 - q0 * q2);
    *gy = 2.0f * (q0 * q1 + q2 * q3);
    *gz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;
}

void EKF::getLinearAcceleration(float ax, float ay, float az, float *lin_ax, float *lin_ay, float *lin_az)
{
    float gx, gy, gz;
    getGravity(&gx, &gy, &gz);
    // Assuming ax, ay, az are in G units (1G = 9.8) or normalized G.
    // If ax is m/s^2, gx should be scaled. User responsibility generally,
    // but commonly libraries return gravity in G (length 1).
    // If input ax is in G, this works.
    *lin_ax = ax - gx;
    *lin_ay = ay - gy;
    *lin_az = az - gz;
}

void EKF::getQuaternion(float *rq0, float *rq1, float *rq2, float *rq3)
{
    *rq0 = q0;
    *rq1 = q1;
    *rq2 = q2;
    *rq3 = q3;
}
