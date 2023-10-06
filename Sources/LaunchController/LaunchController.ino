#include <Arduino.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>
#include <DFPlayer_Mini_Mp3.h>
#include "Control.hpp"
#include "SemiAutoControl.hpp"
#include "AccessLED.hpp"
#include "Button.hpp"
#include "AmpereMonitor.hpp"
#include "VoltageMonitor.hpp"
#include "ThermalMonitor.hpp"


namespace control {
  Control power(PIN_PF5);

  SemiAutoControl shift(PIN_PD4, PIN_PH5, "PlayShift");
  SemiAutoControl fill(PIN_PD5, PIN_PB0, "PlayFill");
  SemiAutoControl dump(PIN_PG1, PIN_PB5, "PlayDump");
  SemiAutoControl oxygen(PIN_PC1, PIN_PB7, "PlayOxygen");
  SemiAutoControl ignition(PIN_PD7, PIN_PH4, "PlayIgnition");
  SemiAutoControl open(PIN_PD6, PIN_PH6, "PlayOpen");
  SemiAutoControl close(PIN_PG0, PIN_PB4, "PlayClose");
  SemiAutoControl purge(PIN_PC0, PIN_PB6, "PlayPurge");

  void handleManualTask();
} // namespace control

namespace indicator {
  AccessLED task(PIN_PK4);

  Control emergencyStop(PIN_PG4);
} // namespace indicator

namespace button {
  Button kill(PIN_PJ1, false);
  Button emergencyStop(PIN_PC4, false);
} // namespace button

namespace sequence {
  void christmasTree();
  void clear();
} // namespace sequence

namespace monitor {
  AmpereMonitor ampereVSW(0x40);
  AmpereMonitor ampere12V(0x41);
  VoltageMonitor voltageVSW(PIN_PF2, 12000.0, 2000.0);
  VoltageMonitor voltage12V(PIN_PF3, 12000.0, 2000.0);
  ThermalMonitor thermal(PIN_PF4, 10000.0);
} // namespace monitor

namespace rs485 {
  Control sendEnableControl(PIN_PA2);
  Control txLED(PIN_PA4);
  Control rxLED(PIN_PA3);

  void enableOutput();
  void disableOutput();
} // namespace rs485


namespace task {
  void monitor();
  void controlSync();
} // namespace task


bool isBusy = false;

void setup() {
  control::power.turnOn();

  // RS485の送信が終わったら割り込みを発生させる
  UCSR1B |= (1 << TXCIE0);

  // FT232RL
  Serial.begin(115200);

  // LTC485
  Serial1.begin(115200);

  // DFPlayer
  Serial2.begin(9600);
  mp3_set_serial(Serial2);
  mp3_set_volume(20);

  Wire.begin();
  monitor::ampereVSW.begin();
  monitor::ampere12V.begin();

  // 音声を再生するタスクたち
  Tasks.add("PlayStartup", [] {mp3_play(100);});
  Tasks.add("PlayShift", [] {mp3_play(110);});
  Tasks.add("PlayFill", [] {mp3_play(111);});
  Tasks.add("PlayDump", [] {mp3_play(112);});
  Tasks.add("PlayOxygen", [] {mp3_play(113);});
  Tasks.add("PlayIgnition", []() {mp3_play(114);});
  Tasks.add("PlayOpen", []() {mp3_play(115);});
  Tasks.add("PlayClose", []() {mp3_play(116);});
  Tasks.add("PlayPurge", []() {mp3_play(117);});

  // シーケンス関係のタスクたち
  Tasks.add("Clear", &sequence::clear);
  Tasks.add("ChristmasTree", &sequence::christmasTree);

  Tasks.add(&task::monitor)->startFps(10);
  Tasks.add(&task::controlSync)->startFps(5);
  Tasks.add(&control::handleManualTask)->startFps(20);

  // クリスマスツリー
  Tasks["ChristmasTree"]->startOnceAfterSec(0.2);
  Tasks["PlayStartup"]->startOnceAfterSec(0.2);
  Tasks["Clear"]->startOnceAfterSec(3.0);
}


void loop() {
  Tasks.update();
}


/// @brief RS485の送信が終わったら送信を無効にするイベントハンドラ
ISR(USART1_TX_vect) {
  rs485::disableOutput();
}



void sequence::christmasTree() {
  // TODO クリスマスツリーはLEDのみ制御する
  control::shift.setAutomaticOn();
  control::fill.setAutomaticOn();
  control::dump.setAutomaticOn();
  control::oxygen.setAutomaticOn();
  control::ignition.setAutomaticOn();
  control::open.setAutomaticOn();
  control::close.setAutomaticOn();
  control::purge.setAutomaticOn();
}


void sequence::clear() {
  control::shift.setAutomaticOff();
  control::fill.setAutomaticOff();
  control::dump.setAutomaticOff();
  control::oxygen.setAutomaticOff();
  control::ignition.setAutomaticOff();
  control::open.setAutomaticOff();
  control::close.setAutomaticOff();
  control::purge.setAutomaticOff();
}


/// @brief 送信を有効にする
void rs485::enableOutput() {
  rs485::sendEnableControl.turnOn();
  rs485::txLED.turnOn();
}


/// @brief 送信を無効にする
void rs485::disableOutput() {
  rs485::sendEnableControl.turnOff();
  rs485::txLED.turnOff();
}


void task::monitor() {
  float ampereVSW_A = monitor::ampereVSW.getAmpere_A();
  float ampereV12_A = monitor::ampere12V.getAmpere_A();
  float voltageVSW_V = monitor::voltageVSW.getVoltage_V();
  float voltage12V_V = monitor::voltage12V.getVoltage_V();
  float powerDissipation_W = ampereVSW_A * voltageVSW_V;
  float thermal_degC = monitor::thermal.getTemperature_degC();

  // Serial.print("IVSW[A]:");
  // Serial.print(ampereVSW_A, 3);
  // Serial.print("\tIV12[A]:");
  // Serial.print(ampereV12_A, 3);
  // Serial.print("\tVVSW[V]:");
  // Serial.print(voltageVSW_V, 3);
  // Serial.print("\tVV12[V]:");
  // Serial.print(voltage12V_V, 3);
  // Serial.print("\tPD[W]:");
  // Serial.print(powerDissipation_W, 3);
  // Serial.print("\tTEMP[degC]:");
  // Serial.print(thermal_degC, 3);
  // Serial.println();
}


void task::controlSync() {
  rs485::enableOutput();
  //HACK テストパケット
  float value = (float)millis() / 1000.0;
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(0xAA), value);
}


void control::handleManualTask() {
  if (button::kill.isPushed()) {
    // 終了処理
    control::power.turnOff();
  }

  // HACK 仮エマスト
  if (!isBusy && button::emergencyStop.isPushed()) {
    isBusy = true;
    mp3_set_volume(20);
    // mp3_play(3);
    // 空襲警報
    mp3_play(102);

    indicator::emergencyStop.turnOn();
    control::close.setAutomaticOn();
    control::dump.setAutomaticOn();
    control::purge.setAutomaticOn();
  }

  // 手動制御
  control::shift.setManual();
  control::fill.setManual();
  control::dump.setManual();
  control::oxygen.setManual();
  control::ignition.setManual();
  control::open.setManual();
  control::close.setManual();
  control::purge.setManual();

  indicator::task.blink();
}