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


namespace power {
  Button killButton(PIN_PJ1, false);
  Control loadSwitch(PIN_PF5);
} // namespace power

namespace control {
  SemiAutoControl safetyArmed(PIN_PC2, PIN_PH7);
  SemiAutoControl sequenceStart(PIN_PC3, PIN_PG3);
  SemiAutoControl emergencyStop(PIN_PC4, PIN_PG4);

  SemiAutoControl shift(PIN_PD4, PIN_PH5);
  SemiAutoControl fill(PIN_PD5, PIN_PB0);
  SemiAutoControl dump(PIN_PG1, PIN_PB5);
  SemiAutoControl oxygen(PIN_PC1, PIN_PB7);
  SemiAutoControl ignition(PIN_PD7, PIN_PH4);
  SemiAutoControl open(PIN_PD6, PIN_PH6);
  SemiAutoControl close(PIN_PG0, PIN_PB4);
  SemiAutoControl purge(PIN_PC0, PIN_PB6);

  void handleManualTask();
} // namespace control

namespace indicator {
  AccessLED task(PIN_PK4);
} // namespace indicator

namespace sequence {
  void christmasTreeOn();
  void christmasTreeOff();
  void emergencyStop();
  void fillSequence();

  bool emergencyStopIsActive = false;
  bool fillSequenceIsActive = false;
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


void setup() {
  power::loadSwitch.turnOn();

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

  // シーケンス関係のタスクたち
  Tasks.add("ChristmasTreeOn", &sequence::christmasTreeOn);
  Tasks.add("ChristmasTreeOff", &sequence::christmasTreeOff);
  Tasks.add("EmergencyStop", &sequence::emergencyStop);
  Tasks.add("FillSequence", &sequence::fillSequence);

  Tasks.add(&task::monitor)->startFps(10);
  Tasks.add(&task::controlSync)->startFps(5);
  Tasks.add(&control::handleManualTask)->startFps(20);

  // クリスマスツリー
  Tasks["ChristmasTreeOn"]->startOnceAfterSec(0.1);
  Tasks["ChristmasTreeOff"]->startOnceAfterSec(3.1);
}


void loop() {
  Tasks.update();
}


/// @brief RS485の送信が終わったら送信を無効にするイベントハンドラ
ISR(USART1_TX_vect) {
  rs485::disableOutput();
}


void sequence::christmasTreeOn() {
  // 0100_Startup.mp3を再生
  mp3_play(100);
  control::safetyArmed.setTestOn();
  control::sequenceStart.setTestOn();
  control::emergencyStop.setTestOn();
  control::shift.setTestOn();
  control::fill.setTestOn();
  control::dump.setTestOn();
  control::oxygen.setTestOn();
  control::ignition.setTestOn();
  control::open.setTestOn();
  control::close.setTestOn();
  control::purge.setTestOn();
}


void sequence::christmasTreeOff() {
  control::safetyArmed.setTestOff();
  control::sequenceStart.setTestOff();
  control::emergencyStop.setTestOff();
  control::shift.setTestOff();
  control::fill.setTestOff();
  control::dump.setTestOff();
  control::oxygen.setTestOff();
  control::ignition.setTestOff();
  control::open.setTestOff();
  control::close.setTestOff();
  control::purge.setTestOff();
}


void sequence::emergencyStop() {
  // 0102_EmergencyStop.mp3
  mp3_play(102);
  control::emergencyStop.setAutomaticOn();
  control::fill.setAutomaticOff();
  control::oxygen.setAutomaticOff();
  control::ignition.setAutomaticOff();
  control::open.setAutomaticOff();
  control::close.setAutomaticOn();
  control::dump.setAutomaticOn();
  control::purge.setAutomaticOn();
}


void sequence::fillSequence() {
  // 0103_SequenceStart.mp3
  mp3_play(103);
  control::sequenceStart.setAutomaticOn();
  control::fill.setAutomaticOn();
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

  Serial.print("IVSW[A]:");
  Serial.print(ampereVSW_A, 3);
  Serial.print("\tIV12[A]:");
  Serial.print(ampereV12_A, 3);
  Serial.print("\tVVSW[V]:");
  Serial.print(voltageVSW_V, 3);
  Serial.print("\tVV12[V]:");
  Serial.print(voltage12V_V, 3);
  Serial.print("\tPD[W]:");
  Serial.print(powerDissipation_W, 3);
  Serial.print("\tTEMP[degC]:");
  Serial.print(thermal_degC, 3);
  Serial.println();
}


void task::controlSync() {
  rs485::enableOutput();
  //HACK テストパケット
  float value = (float)millis() / 1000.0;
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(0xAA), value);
}


void control::handleManualTask() {
  if (power::killButton.isPushed()) {
    // 終了処理
    power::loadSwitch.turnOff();
  }

  control::safetyArmed.setManual();
  control::sequenceStart.setManual();
  control::emergencyStop.setManual();

  // エマスト
  if (!sequence::emergencyStopIsActive && control::emergencyStop.isManualRaised()) {
    sequence::emergencyStopIsActive = true;
    Tasks["EmergencyStop"]->startOnceAfterSec(0.01);
  }

  // 充填シーケンス
  if (!sequence::fillSequenceIsActive && control::sequenceStart.isManualRaised()) {
    sequence::fillSequenceIsActive = true;
    Tasks["FillSequence"]->startOnceAfterSec(1.0);
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