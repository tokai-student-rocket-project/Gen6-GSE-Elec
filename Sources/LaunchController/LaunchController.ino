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

  SemiAutoControl close(PIN_PB4);
  SemiAutoControl dump(PIN_PB5);
  SemiAutoControl purge(PIN_PB6);
}

namespace indicator {
  AccessLED task(PIN_PK4);

  Control emergencyStop(PIN_PG4);
}

namespace button {
  Button kill(PIN_PJ1, false);
  Button emergencyStop(PIN_PC4, false);
}


namespace monitor {
  AmpereMonitor ampereVSW(0x40);
  AmpereMonitor ampere12V(0x41);
  VoltageMonitor voltageVSW(PIN_PF2, 12000.0, 2000.0);
  VoltageMonitor voltage12V(PIN_PF3, 12000.0, 2000.0);
  ThermalMonitor thermal(PIN_PF4, 10000.0);
}

namespace rs485 {
  Control sendEnableControl(PIN_PA2);
  Control txLED(PIN_PA4);
  Control rxLED(PIN_PA3);

  void enableOutput();
  void disableOutput();
}


namespace task {
  void monitor();
  void controlSync();
  void handleManualControl();
}


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

  Wire.begin();
  monitor::ampereVSW.begin();
  monitor::ampere12V.begin();

  Tasks.add(&task::monitor)->startFps(10);
  Tasks.add(&task::controlSync)->startFps(5);
  Tasks.add(&task::handleManualControl)->startFps(20);

  // HACK 動作確認用
  mp3_set_serial(Serial2);
  mp3_set_volume(15);
  mp3_play(100);

  // Tasks.add<EmergencyStop>("EmergencyStop")
  //   ->sync<EmergencyStop>("Dump", [&](TaskRef<EmergencyStop> task) {
  //   task->dump();
  //     });
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


void task::handleManualControl() {
  if (button::kill.isPushed()) {
    // 終了処理
    control::power.turnOff();
  }

  // HACK 仮エマスト
  if (!isBusy && button::emergencyStop.isPushed()) {
    isBusy = true;
    mp3_set_volume(30);
    mp3_play(3);
    indicator::emergencyStop.turnOn();
    control::close.autoSet(HIGH);
    control::dump.autoSet(HIGH);
    control::purge.autoSet(HIGH);
  }

  indicator::task.blink();
}