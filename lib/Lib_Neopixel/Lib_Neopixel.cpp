#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Lib_Neopixel.hpp>

Neopixel::Neopixel(uint8_t pin)
{
    _neopixel = new Adafruit_NeoPixel(1, pin, NEO_GRB + NEO_KHZ800);
}

void Neopixel::init(uint8_t power)
{
    _power = power;
    pinMode(_power, OUTPUT);
    digitalWrite(_power, HIGH);
    _neopixel->begin();
}

void Neopixel::off()
{
    _neopixel->clear();
    _neopixel->setPixelColor(0, _neopixel->Color(0, 0, 0));
    _neopixel->show();
}

void Neopixel::noticedPink()
{
    _neopixel->clear();
    _neopixel->setPixelColor(0, _neopixel->Color(255, 51, 255));
    _neopixel->show();
}

void Neopixel::noticedGreen()
{
    _neopixel->clear();
    _neopixel->setPixelColor(0, _neopixel->Color(0, 150, 0));
    _neopixel->show();
}

void Neopixel::noticedBlue()
{
    _neopixel->clear();
    _neopixel->setPixelColor(0, _neopixel->Color(0, 0, 150));
    _neopixel->show();
}

void Neopixel::noticedRed()
{
    _neopixel->clear();
    _neopixel->setPixelColor(0, _neopixel->Color(255, 0, 0));
    _neopixel->show();
}

void Neopixel::noticedWhite()
{
    _neopixel->clear();
    _neopixel->setPixelColor(0, _neopixel->Color(255, 255, 255));
    _neopixel->show();
}

void Neopixel::noticedTime(bool permissionTime)
{
    if (permissionTime == 1)
    {
        _neopixel->clear();
        _neopixel->setPixelColor(0, _neopixel->Color(255, 0, 0));
        _neopixel->show();
    }
}
void Neopixel::noticedRainbow()
{
    static uint8_t colorIndex = 0;
    _neopixel->clear();

    switch (colorIndex)
    {
    case 0:
        _neopixel->setPixelColor(0, _neopixel->Color(255, 0, 0));
        break; // Red
    case 1:
        _neopixel->setPixelColor(0, _neopixel->Color(255, 165, 0));
        break; // Orange
    case 2:
        _neopixel->setPixelColor(0, _neopixel->Color(255, 255, 0));
        break; // Yellow
    case 3:
        _neopixel->setPixelColor(0, _neopixel->Color(0, 255, 0));
        break; // Green
    case 4:
        _neopixel->setPixelColor(0, _neopixel->Color(0, 0, 255));
        break; // Blue
    case 5:
        _neopixel->setPixelColor(0, _neopixel->Color(75, 0, 130));
        break; // Indigo
    case 6:
        _neopixel->setPixelColor(0, _neopixel->Color(148, 0, 211));
        break; // Violet
    }

    _neopixel->show();

    colorIndex = (colorIndex + 1) % 7;
}

void Neopixel::setBatteryStatus(float voltage)
{
    _neopixel->clear();
    if (voltage >= 13.1)
    {
        // Green
        _neopixel->setPixelColor(0, _neopixel->Color(0, 255, 0));
    }
    else if (voltage >= 11.5)
    {
        // Orange
        _neopixel->setPixelColor(0, _neopixel->Color(255, 165, 0));
    }
    else
    {
        // Red
        _neopixel->setPixelColor(0, _neopixel->Color(255, 0, 0));
    }
    _neopixel->show();
}
