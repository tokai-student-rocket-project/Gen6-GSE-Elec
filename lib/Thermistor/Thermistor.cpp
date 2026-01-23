#include <Thermistor.hpp>

Thermistor::Thermistor(uint8_t pinNumber, float lowerResistance)
{
  _pinNumber = pinNumber;
  _lowerResistance = lowerResistance;
}

float Thermistor::getTemperature_degC()
{
  float readingValue = (float)analogRead(_pinNumber);
  float resistance = _lowerResistance * readingValue / (1024.0 - readingValue);

  float temperatureBar = 1.0 / T0 + 1.0 / B * log(resistance / R0);
  return 1.0 / temperatureBar - 273.15;
}
