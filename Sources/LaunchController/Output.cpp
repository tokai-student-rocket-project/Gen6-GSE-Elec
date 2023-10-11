#include "Output.hpp"


/// @brief コンストラクタ
/// @param pinNumber ピン番号
Output::Output(uint8_t pinNumber) {
  _pinNumber = pinNumber;
  pinMode(_pinNumber, OUTPUT);
  off();
}


/// @brief 出力をHIGHにする
void Output::on() {
  digitalWrite(_pinNumber, HIGH);
}


/// @brief 出力をLOWにする
void Output::off() {
  digitalWrite(_pinNumber, LOW);
}


/// @brief 出力を入れ替える（HIGHならLOW LOWならHIGH）
void Output::toggle() {
  digitalWrite(_pinNumber, !digitalRead(_pinNumber));
}


/// @brief 一瞬だけON
void Output::blink() {
  // TODO 一定時間で消灯する処理を追加する（仮で点滅）
  digitalWrite(_pinNumber, !digitalRead(_pinNumber));
}


/// @brief 出力を設定する
void Output::set(bool isHigh) {
  digitalWrite(_pinNumber, isHigh ? HIGH : LOW);
}
