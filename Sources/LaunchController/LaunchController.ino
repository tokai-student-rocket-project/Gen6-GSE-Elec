#include <Arduino.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>
#include <DFPlayer_Mini_Mp3.h>
#include "TM1637.hpp"
#include "Input.hpp"
#include "Output.hpp"
#include "SemiAutoControl.hpp"
#include "PowerMonitor.hpp"
#include "Thermistor.hpp"


namespace power {
  Input killButton(PIN_PJ1, false);
  Output loadSwitch(PIN_PF5);
  Output powerLamp(PIN_PG5);
  Output lowVoltageLamp(PIN_PK7);

  PowerMonitor input(0x40);
  PowerMonitor bus12(0x41);
  Thermistor thermal(PIN_PF4, 10000.0);

  void measureTask();
} // namespace power

namespace control {
  SemiAutoControl safetyArmed(PIN_PC2, false, PIN_PH7);
  SemiAutoControl sequenceStart(PIN_PC3, false, PIN_PG3);
  SemiAutoControl emergencyStop(PIN_PC4, true, PIN_PG4);

  Input confirm1(PIN_PC7, false);
  Input confirm2(PIN_PC6, false);
  Input confirm3(PIN_PC5, false);

  SemiAutoControl shift(PIN_PD4, false, PIN_PH5);
  SemiAutoControl fill(PIN_PD5, false, PIN_PB0);
  SemiAutoControl dump(PIN_PG1, false, PIN_PB5);
  SemiAutoControl oxygen(PIN_PC1, false, PIN_PL4);
  SemiAutoControl igniter(PIN_PD7, false, PIN_PH4);
  SemiAutoControl open(PIN_PD6, false, PIN_PH6);
  SemiAutoControl close(PIN_PG0, false, PIN_PB4);
  SemiAutoControl purge(PIN_PC0, false, PIN_PB6);

  Output shiftFB(PIN_PE3);
  Output fillFB(PIN_PE5);
  Output dumpFB(PIN_PE7);
  Output oxygenFB(PIN_PH3);
  Output igniterFB(PIN_PE2);
  Output openFB(PIN_PE4);
  Output closeFB(PIN_PE6);
  Output purgeFB(PIN_PH2);

  Output statusLamp(PIN_PK4);
  void handleManualTask();

  const String FILL_START = "fill-start";
  const String FILL_STOP = "fill-stop";
  const String OXYGEN_START = "oxygen-start";
  const String OXYGEN_STOP = "oxygen-stop";
  const String IGNITER_START = "igniter-start";
  const String IGNITER_STOP = "igniter-stop";
  const String OPEN_START = "open-start";
  const String PLAY_MUSIC = "play-music";

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
  void emergencyStop();
  void peacefulStop();
  void fill();
  void ignition();

  uint32_t sequenceStartRiseCount = 0;

  bool emergencyStopSequenceIsActive = false;
  bool fillSequenceIsActive = false;
  bool ignitionSequenceIsActive = false;
  bool canConfirm = false;
} // namespace sequence

namespace n2o {
  TM1637 tm1637(PIN_PK0, PIN_PK1);
} // namespace n2o

namespace error {
  // HACK LEDだけでなく処理もする
  Output statusLamp(PIN_PK6);
} // namespace caution


namespace communication {
  enum class Packet : uint8_t {
    CONTROL_SYNC,
    FEEDBACK_SYNC,
    PRESSURE_SYNC,
    COM_CHECK_L_TO_S,
    COM_CHECK_S_TO_L
  };

  Output sendEnableControl(PIN_PA2);
  Output accessLamp(PIN_PA4);

  void enableOutput();
  void disableOutput();

  void sendControlSync();
  void sendComCheck();
  void onFeedbackSyncReceived(uint8_t state);
  void onPressureSyncReceived(float pressure);
  void onComCheckReceived();

  Output statusLamp(PIN_PK5);
} // namespace communication


void setup() {
  power::loadSwitch.on();
  power::powerLamp.on();


  // FT232RL (USB)
  Serial.begin(115200);

  // LTC485 (RS485)
  Serial1.begin(115200);

  // DFPlayer (Audio)
  Serial2.begin(9600);
  mp3_set_serial(Serial2);
  mp3_stop();
  mp3_set_volume(30);

  // TM1637 (7SEG)
  n2o::tm1637.initialize();

  // INA219 (Power)
  Wire.begin();
  power::input.begin();
  power::bus12.begin();


  Tasks.add(&power::measureTask)->startFps(10);
  Tasks.add(&control::handleManualTask)->startFps(50);


  Tasks.add(&communication::sendControlSync)->startFps(50);
  Tasks.add(&communication::sendComCheck)->startFps(2);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::FEEDBACK_SYNC), &communication::onFeedbackSyncReceived);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::PRESSURE_SYNC), &communication::onPressureSyncReceived);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_S_TO_L), &communication::onComCheckReceived);

  // シーケンス関係のタスクたち
  Tasks.add(control::FILL_START, &control::setFillStart);
  Tasks.add(control::FILL_STOP, &control::setFillStop);
  Tasks.add(control::OXYGEN_START, &control::setOxygenStart);
  Tasks.add(control::OXYGEN_STOP, &control::setOxygenStop);
  Tasks.add(control::IGNITER_START, &control::setIgniterStart);
  Tasks.add(control::IGNITER_STOP, &control::setIgniterStop);
  Tasks.add(control::OPEN_START, &control::setOpenStart);
  Tasks.add(control::PLAY_MUSIC, [] {mp3_play(9);});


  control::setChristmasTreeStart();
  Tasks.add(&control::setChristmasTreeStop)->startOnceAfterSec(3.0);
  mp3_play(11);
}


void loop() {
  MsgPacketizer::parse();
  Tasks.update();
}


/// @brief 送信を有効にする
void communication::enableOutput() {
  communication::sendEnableControl.on();
  communication::accessLamp.on();
}


/// @brief 送信を無効にする
void communication::disableOutput() {
  communication::sendEnableControl.off();
  communication::accessLamp.off();
}


void power::measureTask() {
  bool isLowVoltage = power::input.getVoltage_V() < 17.0;
  bool isOverloadedInput = power::input.getAmpere_A() > 3.0;
  bool isOverloadedBus = power::bus12.getAmpere_A() > 3.0;
  bool isOverheated = power::thermal.getTemperature_degC() > 100.0;

  power::lowVoltageLamp.set(isLowVoltage);

  Serial.println(power::thermal.getTemperature_degC());

  if (isOverloadedInput || isOverloadedBus || isOverheated) {
    // HACK エラー
    error::statusLamp.on();
  }
}


void communication::sendControlSync() {
  uint8_t state = (control::shift.isRaised() << 0) | (control::fill.isRaised() << 1) | (control::dump.isRaised() << 2) | (control::oxygen.isRaised() << 3) | (control::igniter.isRaised() << 4) | (control::open.isRaised() << 5) | (control::close.isRaised() << 6) | (control::purge.isRaised() << 7);

  communication::enableOutput();
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::CONTROL_SYNC), state);
  Serial1.flush();
  communication::disableOutput();
}


void communication::sendComCheck() {
  communication::enableOutput();
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_S));
  Serial1.flush();
  communication::disableOutput();
}


void communication::onFeedbackSyncReceived(uint8_t state) {
  control::shiftFB.set(state & (1 << 0));
  control::fillFB.set(state & (1 << 1));
  control::dumpFB.set(state & (1 << 2));
  control::oxygenFB.set(state & (1 << 3));
  control::igniterFB.set(state & (1 << 4));
  control::openFB.set(state & (1 << 5));
  control::closeFB.set(state & (1 << 6));
  control::purgeFB.set(state & (1 << 7));

  communication::statusLamp.blink();
}


void communication::onPressureSyncReceived(float pressure) {
  n2o::tm1637.displayNumber(pressure);

  communication::statusLamp.blink();
}


void communication::onComCheckReceived() {
  communication::statusLamp.blink();
}


void control::handleManualTask() {
  control::statusLamp.blink();

  if (power::killButton.isHigh()) {
    mp3_play(12);
    power::powerLamp.off();
    delay(500);
    power::powerLamp.on();
    delay(500);
    power::powerLamp.off();
    delay(500);
    power::powerLamp.on();
    delay(500);
    power::powerLamp.off();
    delay(500);
    power::powerLamp.on();
    delay(500);
    power::powerLamp.off();
    delay(500);
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
    if (sequence::sequenceStartRiseCount == 0) {
      if (!(sequence::emergencyStopSequenceIsActive || sequence::fillSequenceIsActive || sequence::ignitionSequenceIsActive)) {
        sequence::fill();
      }
      // 充填確認スイッチの代替
      else if (sequence::canConfirm) {
        sequence::ignition();
      }
      else {
        sequence::peacefulStop();
      }
    }

    sequence::sequenceStartRiseCount++;
  }
  else {
    sequence::sequenceStartRiseCount = 0;
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


void sequence::emergencyStop() {
  // 重複実行防止
  if (sequence::emergencyStopSequenceIsActive) return;

  sequence::emergencyStopSequenceIsActive = true;

  sequence::fillSequenceIsActive = false;
  sequence::ignitionSequenceIsActive = false;
  sequence::canConfirm = false;

  control::emergencyStop.setAutomaticOn();
  mp3_play(3); // 0102_emergencyStop.mp3

  control::sequenceStart.setAutomaticOff();

  Tasks[control::PLAY_MUSIC]->stop();
  Tasks[control::FILL_START]->stop();
  Tasks[control::OXYGEN_START]->stop();
  Tasks[control::IGNITER_START]->stop();
  Tasks[control::FILL_STOP]->stop();
  Tasks[control::OPEN_START]->stop();
  Tasks[control::OXYGEN_STOP]->stop();
  Tasks[control::IGNITER_STOP]->stop();

  control::setEmergencyStop();
}


void sequence::peacefulStop() {
  control::sequenceStart.setAutomaticOff();

  sequence::emergencyStopSequenceIsActive = false;
  sequence::fillSequenceIsActive = false;
  sequence::ignitionSequenceIsActive = false;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOff();
  control::emergencyStop.setAutomaticOff();

  Tasks[control::PLAY_MUSIC]->stop();
  Tasks[control::FILL_START]->stop();
  Tasks[control::OXYGEN_START]->stop();
  Tasks[control::IGNITER_START]->stop();
  Tasks[control::FILL_STOP]->stop();
  Tasks[control::OPEN_START]->stop();
  Tasks[control::OXYGEN_STOP]->stop();
  Tasks[control::IGNITER_STOP]->stop();

  mp3_stop();
  control::setPeacefulStop();
}


void sequence::fill() {
  // 重複実行防止
  if (sequence::fillSequenceIsActive) return;

  // エマスト中は充填シーケンスを始めない
  if (sequence::emergencyStopSequenceIsActive) return;

  // シーケンス開始時点で充填確認されていたらエラーを吐く
  if (control::confirm1.isHigh() || control::confirm2.isHigh() || control::confirm3.isHigh()) {
    // HACK エラー
    sequence::peacefulStop();
    error::statusLamp.on();
    return;
  }

  sequence::fillSequenceIsActive = true;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOn();
  mp3_play(10);

  Tasks[control::PLAY_MUSIC]->startOnceAfterSec(15.0);
  Tasks[control::FILL_START]->startOnceAfterSec(24.0);
}


void sequence::ignition() {
  // 重複実行防止
  if (sequence::ignitionSequenceIsActive) return;

  // エマスト中は点火シーケンスを始めない
  if (sequence::emergencyStopSequenceIsActive) return;

  // 充填開始前は点火シーケンスを始めない
  if (!control::fill.isAutomaticRaised()) return;

  // 手動のFILLがONの間は点火シーケンスを始めない
  if (control::fill.isManualRaised()) return;

  sequence::ignitionSequenceIsActive = true;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOn();
  mp3_play(4); // 0104_ignitionSequenceStart

  Tasks[control::OXYGEN_START]->startOnceAfterSec(3.0);

  Tasks[control::IGNITER_START]->startOnceAfterSec(6.0);

  Tasks[control::FILL_STOP]->startOnceAfterSec(10.0);
  Tasks[control::OPEN_START]->startOnceAfterSec(10.0);

  Tasks[control::OXYGEN_STOP]->startOnceAfterSec(10.5);
  Tasks[control::IGNITER_STOP]->startOnceAfterSec(10.5);
}


void control::setChristmasTreeStart() {
  n2o::tm1637.displayNumber(8.8);
  error::statusLamp.setTestOn();
  power::lowVoltageLamp.setTestOn();
  control::statusLamp.setTestOn();
  communication::accessLamp.setTestOn();
  communication::statusLamp.setTestOn();
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
  control::shiftFB.setTestOn();
  control::fillFB.setTestOn();
  control::dumpFB.setTestOn();
  control::oxygenFB.setTestOn();
  control::igniterFB.setTestOn();
  control::openFB.setTestOn();
  control::closeFB.setTestOn();
  control::purgeFB.setTestOn();
}


void control::setChristmasTreeStop() {
  n2o::tm1637.clearDisplay();();
  error::statusLamp.setTestOff();
  power::lowVoltageLamp.setTestOff();
  control::statusLamp.setTestOff();
  communication::accessLamp.setTestOff();
  communication::statusLamp.setTestOff();
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
  control::shiftFB.setTestOff();
  control::fillFB.setTestOff();
  control::dumpFB.setTestOff();
  control::oxygenFB.setTestOff();
  control::igniterFB.setTestOff();
  control::openFB.setTestOff();
  control::closeFB.setTestOff();
  control::purgeFB.setTestOff();
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
  sequence::canConfirm = true;
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
