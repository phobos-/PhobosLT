#include <stdint.h>

class KalmanFilter {
   public:
    KalmanFilter();
    float filter(uint16_t z, uint16_t u);
    float lastMeasurement();
    void setMeasurementNoise(float noise);
    void setProcessNoise(float noise);

   private:
    float R;  // noise power desirable
    float Q;  // noise power estimated
    float A;
    float C;
    float B;
    float cov;  // NaN
    float x;    // NaN -- estimated signal without noise
};
