#include "TM1637.hpp"


TM1637::TM1637(uint8_t clockPinNumber, uint8_t dataPinNumber) {
  _clockPinNumber = clockPinNumber;
  _dataPinNumber = dataPinNumber;
  pinMode(_clockPinNumber, OUTPUT);
  pinMode(_dataPinNumber, OUTPUT);
}


void TM1637::initialize() {
  clearDisplay();
}


void TM1637::clearDisplay() {
  display(0, 0b0100'0000);
  display(1, 0b0100'0000);
}


void TM1637::start() {
  digitalWrite(_clockPinNumber, HIGH);
  digitalWrite(_dataPinNumber, HIGH);
  digitalWrite(_dataPinNumber, LOW);
  digitalWrite(_clockPinNumber, LOW);
}


void TM1637::stop() {
  digitalWrite(_clockPinNumber, LOW);
  digitalWrite(_dataPinNumber, LOW);
  digitalWrite(_clockPinNumber, HIGH);
  digitalWrite(_dataPinNumber, HIGH);
}


void TM1637::writeByte(int8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(_clockPinNumber, LOW);

    if (data & 0x01) {
      digitalWrite(_dataPinNumber, HIGH);
    }
    else {
      digitalWrite(_dataPinNumber, LOW);
    }

    data >>= 1;
    digitalWrite(_clockPinNumber, HIGH);
  }

  digitalWrite(_clockPinNumber, LOW);
  digitalWrite(_dataPinNumber, HIGH);
  digitalWrite(_clockPinNumber, HIGH);
  pinMode(_dataPinNumber, INPUT);

  bitDelay();
  uint8_t ack = digitalRead(_dataPinNumber);

  if (ack == 0) {
    pinMode(_dataPinNumber, OUTPUT);
    digitalWrite(_dataPinNumber, LOW);
  }

  bitDelay();
  pinMode(_dataPinNumber, OUTPUT);
  bitDelay();
}


int8_t TM1637::segment(int8_t data, bool includesDot) {
  if (data >= 0 && data <= 9) {
    return numberSegments[data] | (includesDot ? 0b1000'0000 : 0b0000'0000);
  }

  return 0b0100'0000;
}


void TM1637::display(uint8_t address, int8_t data) {
  start();
  writeByte(0b0100'01'00);
  stop();

  start();
  writeByte(address | 0b1100'0000);
  writeByte(data);
  stop();

  start();
  writeByte(0b1000'1'111);
  stop();
}


void TM1637::displayNumber(float value) {
  int number = round(fabs(value) * 10);

  display(0, segment((number % 10), false));
  display(1, segment(((number / 10) % 10), true));
}


void TM1637::bitDelay() {
  delayMicroseconds(50);
}
