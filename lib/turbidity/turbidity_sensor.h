#ifndef TURBIDITY_SENSOR_H
#define TURBIDITY_SENSOR_H
#include <Arduino.h>

class TurbiditySensor {
    private:
        uint8_t analogPin;
        
    public:
        TurbiditySensor(uint8_t pin);
        void begin();
        float readVoltage();
        float readNTU();
};

#endif // TURBIDITY_SENSOR_H