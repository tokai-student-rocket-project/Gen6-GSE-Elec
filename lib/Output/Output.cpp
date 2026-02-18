#include <Output.hpp>

void Output::updateOutput() {
  digitalWrite(_pinNumber, _isHigh || _testIsHigh);
}

/// @brief コンストラクタ
/// @param pinNumber ピン番号
Output::Output(uint8_t pinNumber) {
  _pinNumber = pinNumber;
  pinMode(_pinNumber, OUTPUT);
}

/// @brief 出力をHIGHにする
void Output::on() {
  _isHigh = true;
  updateOutput();
}

/// @brief 出力をLOWにする
void Output::off() {
  _isHigh = false;
  updateOutput();
}

/// @brief 出力を入れ替える（HIGHならLOW LOWならHIGH）
void Output::toggle() {
  _isHigh = !_isHigh;
  updateOutput();
}

/// @brief 一瞬だけON
void Output::blink() { pulse(50); }

/// @brief 出力を設定する
void Output::set(bool isHigh) {
  _isHigh = isHigh;
  updateOutput();
}

/// @brief 一定時間だけ点灯させる
void Output::pulse(uint16_t duration_ms) {
  _isHigh = true;
  _pulseDuration = duration_ms;
  _lastPulseTime = millis();
  updateOutput();
}

/// @brief 内部状態の更新（ループ内で呼ぶ必要がある）
void Output::update() {
  if (_pulseDuration > 0) {
    if (millis() - _lastPulseTime >= _pulseDuration) {
      _pulseDuration = 0;
      _isHigh = false;
      updateOutput();
    }
  }
}

bool Output::isHigh() { return _isHigh; }

/// @brief テストの出力をオンに設定する
void Output::setTestOn() {
  _testIsHigh = true;
  updateOutput();
}

/// @brief テストの出力をオフに設定する
void Output::setTestOff() {
  _testIsHigh = false;
  updateOutput();
}
