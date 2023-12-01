#include <Arduino.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>
#include <DFPlayer_Mini_Mp3.h>
#include "Input.hpp"
#include "Output.hpp"
#include "SemiAutoControl.hpp"
#include "PowerMonitor.hpp"
#include "Thermistor.hpp"


namespace power {
  Input killButton(PIN_PJ1);
  Output loadSwitch(PIN_PF5);
  Output lowVoltageLamp(PIN_PK7);
} // namespace power

namespace control {
  SemiAutoControl safetyArmed(PIN_PC2, PIN_PH7);
  SemiAutoControl sequenceStart(PIN_PC3, PIN_PG3);
  SemiAutoControl emergencyStop(PIN_PC4, PIN_PG4);

  Input confirm1(PIN_PC7);
  Input confirm2(PIN_PC6);
  Input confirm3(PIN_PC5);

  SemiAutoControl shift(PIN_PD4, PIN_PH5);
  SemiAutoControl fill(PIN_PD5, PIN_PB0);
  SemiAutoControl dump(PIN_PG1, PIN_PB5);
  SemiAutoControl oxygen(PIN_PC1, PIN_PL4);
  SemiAutoControl igniter(PIN_PD7, PIN_PH4);
  SemiAutoControl open(PIN_PD6, PIN_PH6);
  SemiAutoControl close(PIN_PG0, PIN_PB4);
  SemiAutoControl purge(PIN_PC0, PIN_PB6);

  void handleManualTask();

  void setChristmasTreeStart();
  void setChristmasTreeStop();
  void setEmergencyStop();
  void setPeacefulStop();
  void setFillStart();
  void setFillStop();
  void setOxygenStart();
  void setOxygenStop();
  void setIgniterStart();
  void setIgniterStop();
  void setOpenStart();
} // namespace control

namespace sequence {
  void christmasTree();
  void emergencyStop();
  void peacefulStop();
  void fill();
  void ignition();

  bool emergencyStopSequenceIsActive = false;
  bool fillSequenceIsActive = false;
  bool ignitionSequenceIsActive = false;
  bool isReadyToIgnition = false;
} // namespace sequence

namespace monitor {
  PowerMonitor input(0x40);
  PowerMonitor bus12(0x41);
  Thermistor thermal(PIN_PF4, 10000.0);

  void measureTask();
} // namespace monitor

namespace rs485 {
  Output sendEnableControl(PIN_PA2);
  Output accessLamp(PIN_PA4);

  void enableOutput();
  void disableOutput();
} // namespace rs485

namespace task {
  const String CHRISTMAS_TREE_STOP = "christmas-tree-stop";
  const String FILL_START = "fill-start";
  const String FILL_STOP = "fill-stop";
  const String OXYGEN_START = "oxygen-start";
  const String OXYGEN_STOP = "oxygen-stop";
  const String IGNITER_START = "igniter-start";
  const String IGNITER_STOP = "igniter-stop";
  const String OPEN_START = "open-start";
  const String PLAY_MUSIC = "play-music";

  Output accessLamp(PIN_PK4);

  void controlSync();
} // namespace task

namespace error {
  // HACK LEDだけでなく処理もする
  Output statusLamp(PIN_PK6);
} // namespace caution


namespace satelliteController {
  Output statusLamp(PIN_PK5);
} // namespace satelliteController


void setup() {
  power::loadSwitch.on();

  // RS485の送信が終わったら割り込みを発生させる
  UCSR1B |= (1 << TXCIE0);

  // FT232RL
  Serial.begin(115200);

  // LTC485
  Serial1.begin(115200);

  // DFPlayer
  Serial2.begin(9600);
  mp3_set_serial(Serial2);
  mp3_stop();
  mp3_set_volume(30);

  Wire.begin();
  monitor::input.begin();
  monitor::bus12.begin();

  // シーケンス関係のタスクたち
  Tasks.add(task::CHRISTMAS_TREE_STOP, &control::setChristmasTreeStop);
  Tasks.add(task::FILL_START, &control::setFillStart);
  Tasks.add(task::FILL_STOP, &control::setFillStop);
  Tasks.add(task::OXYGEN_START, &control::setOxygenStart);
  Tasks.add(task::OXYGEN_STOP, &control::setOxygenStop);
  Tasks.add(task::IGNITER_START, &control::setIgniterStart);
  Tasks.add(task::IGNITER_STOP, &control::setIgniterStop);
  Tasks.add(task::OPEN_START, &control::setOpenStart);
  Tasks.add(task::PLAY_MUSIC, [] {mp3_play(9);});

  Tasks.add(&monitor::measureTask)->startFps(10);
  Tasks.add(&task::controlSync)->startFps(5);
  Tasks.add(&control::handleManualTask)->startFps(20);

  sequence::christmasTree();
}


void loop() {
  Tasks.update();
}


/// @brief RS485の送信が終わったら送信を無効にするイベントハンドラ
ISR(USART1_TX_vect) {
  rs485::disableOutput();
}


/// @brief 送信を有効にする
void rs485::enableOutput() {
  rs485::sendEnableControl.on();
  rs485::accessLamp.on();
}


/// @brief 送信を無効にする
void rs485::disableOutput() {
  rs485::sendEnableControl.off();
  rs485::accessLamp.off();
}


void monitor::measureTask() {
  float ampereVSW_A = monitor::input.getAmpere_A();
  float ampereV12_A = monitor::bus12.getAmpere_A();
  float voltageVSW_V = monitor::input.getVoltage_V();
  float voltage12V_V = monitor::bus12.getVoltage_V();
  float powerDissipation_W = monitor::input.getPower_W();
  float thermal_degC = monitor::thermal.getTemperature_degC();

  Serial.print("INPUT[A]:");
  Serial.print(ampereVSW_A, 3);
  Serial.print("\tBUS12[A]:");
  Serial.print(ampereV12_A, 3);
  Serial.print("\tINPUT[V]:");
  Serial.print(voltageVSW_V, 3);
  Serial.print("\tBUS12[V]:");
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
  task::accessLamp.blink();

  if (power::killButton.isHigh()) {
    // 終了処理
    power::loadSwitch.off();
  }

  // セーフティー
  // Armedでなければこの時点で終わり
  control::safetyArmed.setManual();
  if (!control::safetyArmed.isManualRaised()) {
    // シーケンスが進行中なら穏便ストップ
    if (sequence::emergencyStopSequenceIsActive
      || sequence::fillSequenceIsActive
      || sequence::ignitionSequenceIsActive) {
      sequence::peacefulStop();
    }

    return;
  }


  // エマスト
  control::emergencyStop.setManual();
  if (control::emergencyStop.isManualRaised()) {
    sequence::emergencyStop();
  }

  // 充填シーケンス
  control::sequenceStart.setManual();
  if (control::sequenceStart.isManualRaised()) {
    sequence::fill();
  }

  // 点火シーケンス
  if ((control::confirm1.isHigh() && control::confirm2.isHigh())
    || (control::confirm2.isHigh() && control::confirm3.isHigh())
    || (control::confirm3.isHigh() && control::confirm1.isHigh())) {
    sequence::ignition();
  }

  // 手動制御
  control::shift.setManual();
  control::fill.setManual();
  control::dump.setManual();
  control::oxygen.setManual();
  control::igniter.setManual();
  control::open.setManual();
  control::close.setManual();
  control::purge.setManual();
}


void sequence::christmasTree() {
  // mp3_play(100); // 0100_startup.mp3
  control::setChristmasTreeStart();

  Tasks[task::CHRISTMAS_TREE_STOP]->startOnceAfterSec(3.0);
}


void sequence::emergencyStop() {
  // 重複実行防止
  if (sequence::emergencyStopSequenceIsActive) return;
  sequence::emergencyStopSequenceIsActive = true;

  sequence::fillSequenceIsActive = false;
  sequence::ignitionSequenceIsActive = false;

  control::emergencyStop.setAutomaticOn();
  mp3_play(3); // 0102_emergencyStop.mp3

  control::sequenceStart.setAutomaticOff();

  Tasks[task::PLAY_MUSIC]->stop();
  Tasks[task::FILL_START]->stop();
  Tasks[task::OXYGEN_START]->stop();
  Tasks[task::IGNITER_START]->stop();
  Tasks[task::FILL_STOP]->stop();
  Tasks[task::OPEN_START]->stop();
  Tasks[task::OXYGEN_STOP]->stop();
  Tasks[task::IGNITER_STOP]->stop();

  control::setEmergencyStop();
}


void sequence::peacefulStop() {
  control::sequenceStart.setAutomaticOff();

  sequence::emergencyStopSequenceIsActive = false;
  sequence::fillSequenceIsActive = false;
  sequence::ignitionSequenceIsActive = false;

  control::sequenceStart.setAutomaticOff();
  control::emergencyStop.setAutomaticOff();

  Tasks[task::PLAY_MUSIC]->stop();
  Tasks[task::FILL_START]->stop();
  Tasks[task::OXYGEN_START]->stop();
  Tasks[task::IGNITER_START]->stop();
  Tasks[task::FILL_STOP]->stop();
  Tasks[task::OPEN_START]->stop();
  Tasks[task::OXYGEN_STOP]->stop();
  Tasks[task::IGNITER_STOP]->stop();

  mp3_stop();
  control::setPeacefulStop();
}


void sequence::fill() {
  // 重複実行防止
  if (sequence::fillSequenceIsActive) return;
  sequence::fillSequenceIsActive = true;

  // エマスト中は充填シーケンスを始めない
  if (sequence::emergencyStopSequenceIsActive) return;

  // シーケンス開始時点で充填確認されていたらエラーを吐く
  if (control::confirm1.isHigh() || control::confirm2.isHigh() || control::confirm3.isHigh()) {
    // HACK エラー
    sequence::peacefulStop();
    error::statusLamp.on();
    return;
  }

  control::sequenceStart.setAutomaticOn();
  mp3_play(10);

  Tasks[task::PLAY_MUSIC]->startOnceAfterSec(15.0);
  Tasks[task::FILL_START]->startOnceAfterSec(24.0);
}


void sequence::ignition() {
  // 重複実行防止
  if (sequence::ignitionSequenceIsActive) return;
  sequence::ignitionSequenceIsActive = true;

  // エマスト中は点火シーケンスを始めない
  if (sequence::emergencyStopSequenceIsActive) return;

  // 充填開始前は点火シーケンスを始めない
  if (!control::fill.isAutomaticRaised()) return;

  // 手動のFILLがONの間は点火シーケンスを始めない
  if (control::fill.isManualRaised()) return;

  control::sequenceStart.setAutomaticOn();
  mp3_play(4); // 0104_ignitionSequenceStart

  Tasks[task::OXYGEN_START]->startOnceAfterSec(3.0);

  Tasks[task::IGNITER_START]->startOnceAfterSec(6.0);

  Tasks[task::FILL_STOP]->startOnceAfterSec(10.0);
  Tasks[task::OPEN_START]->startOnceAfterSec(10.0);

  Tasks[task::OXYGEN_STOP]->startOnceAfterSec(10.5);
  Tasks[task::IGNITER_STOP]->startOnceAfterSec(10.5);
}


void control::setChristmasTreeStart() {
  error::statusLamp.setTestOn();
  power::lowVoltageLamp.setTestOn();
  task::accessLamp.setTestOn();
  satelliteController::statusLamp.setTestOn();
  rs485::accessLamp.setTestOn();
  control::safetyArmed.setTestOn();
  control::sequenceStart.setTestOn();
  control::emergencyStop.setTestOn();
  control::shift.setTestOn();
  control::fill.setTestOn();
  control::dump.setTestOn();
  control::oxygen.setTestOn();
  control::igniter.setTestOn();
  control::open.setTestOn();
  control::close.setTestOn();
  control::purge.setTestOn();
}


void control::setChristmasTreeStop() {
  error::statusLamp.setTestOff();
  power::lowVoltageLamp.setTestOff();
  task::accessLamp.setTestOff();
  satelliteController::statusLamp.setTestOff();
  rs485::accessLamp.setTestOff();
  control::safetyArmed.setTestOff();
  control::sequenceStart.setTestOff();
  control::emergencyStop.setTestOff();
  control::shift.setTestOff();
  control::fill.setTestOff();
  control::dump.setTestOff();
  control::oxygen.setTestOff();
  control::igniter.setTestOff();
  control::open.setTestOff();
  control::close.setTestOff();
  control::purge.setTestOff();
}


void control::setEmergencyStop() {
  control::fill.setAutomaticOff();
  control::oxygen.setAutomaticOff();
  control::igniter.setAutomaticOff();
  control::open.setAutomaticOff();
  control::close.setAutomaticOn();
  control::dump.setAutomaticOn();
  control::purge.setAutomaticOn();
}


void control::setPeacefulStop() {
  control::fill.setAutomaticOff();
  control::oxygen.setAutomaticOff();
  control::igniter.setAutomaticOff();
  control::open.setAutomaticOff();
  control::close.setAutomaticOff();
  control::dump.setAutomaticOff();
  control::purge.setAutomaticOff();
}


void control::setFillStart() {
  control::fill.setAutomaticOn();
}


void control::setFillStop() {
  control::fill.setAutomaticOff();
}


void control::setOxygenStart() {
  control::oxygen.setAutomaticOn();
}


void control::setOxygenStop() {
  control::oxygen.setAutomaticOff();
}


void control::setIgniterStart() {
  control::igniter.setAutomaticOn();
}


void control::setIgniterStop() {
  control::igniter.setAutomaticOff();
}


void control::setOpenStart() {
  control::open.setAutomaticOn();
}
