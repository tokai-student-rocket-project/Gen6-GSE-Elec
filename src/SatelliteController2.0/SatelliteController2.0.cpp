#include "Input.hpp"
#include "Output.hpp"
#include "PowerMonitor.hpp"
#include "SemiAutoControl.hpp"
#include "SolenoidMonitor_2.0.hpp"
#include "TM1637.hpp"
#include "Thermistor.hpp"
#include "VESIM10_2.0.hpp"
#include <Arduino.h>
#include <MsgPacketizer.h>
#include <TaskManager.h>

namespace power
{
  // キルスイッチ入力 (CTL_KILL): 電源を落とすためのボタン
  Input killButton(PIN_PJ1, false);
  // 電源供給用ロードスイッチ (CTL_POWER): 各部への電源供給を制御
  Output loadSwitch(PIN_PF5);
  // 電源ランプ (LED_POWER): 電源がONであることを示すLED
  Output powerLamp(PIN_PG5);
  // 低電圧警告ランプ (LED_LOW_VOLTAGE): 入力電圧が低下した際に点灯
  Output lowVoltageLamp(PIN_PK7);

  // 入力系統の電力・電流モニタ (I2Cアドレス: 0x40)
  PowerMonitor input(0x40);
  // 12Vバス系統の電力・電流モニタ (I2Cアドレス: 0x41)
  PowerMonitor bus12(0x41);
  // 温度監視用サーミスタ (10kΩ基準): 内部レギュレーターの発熱を監視
  Thermistor thermal(PIN_PF4, 10000.0);

  // 電源や温度の測定値を定期的にチェックするタスク
  void measureTask();
} // namespace power

namespace control
{
  // 安全装置（セーフティ）の物理スイッチとLED。ONにしないと電磁弁は動かない
  SemiAutoControl safetyArmed(PIN_PH7, true, PIN_PG3); // CTL_SAFETY, LED_SAFTEY

  // 各種電磁弁等の制御出力ピン
  Output shift(PIN_PD4);   // シフト弁制御 (CTL_SHIFT)
  Output fill(PIN_PD7);    // FILL弁制御 (CTL_FILL)
  Output dump(PIN_PD6);    // DUMP弁制御 (CTL_DUMP)
  Output oxygen(PIN_PG0);  // O2弁制御 (CTL_O2)
  Output igniter(PIN_PD5); // 点火装置制御 (CTL_IGNITOR)
  Output open(PIN_PG1);    // 主流路弁開制御 (CTL_OPEN)
  Output close(PIN_PC0);   // 主流路弁閉制御 (CTL_CLOSE)
  Output purge(PIN_PC2);   // パージ弁制御 (CTL_PURGE)

  // 手動パージ確認用のスイッチとLED (現在はコメントアウトされたパージスイッチの代わりにチェックとして使用)
  SemiAutoControl check(PIN_PC1, false, PIN_PB6); // CTL_CHECK, LED_CHECK

  // 各種制御状態のフィードバック(確認)用LED出力ピン
  Output shiftFB(PIN_PH5);   // シフト弁状態表示用LED (LED_FB_SHIFT)
  Output fillFB(PIN_PB0);    // FILL弁状態表示用LED (LED_FB_FILL)
  Output dumpFB(PIN_PB5);    // DUMP弁状態表示用LED (LED_FB_DUMP)
  Output oxygenFB(PIN_PH0);  // O2弁状態表示用LED (LED_FB_O2)
  Output igniterFB(PIN_PH4); // 点火状態表示用LED (LED_FB_IGNITOR)
  Output openFB(PIN_PH6);    // 主流路弁開状態表示用LED (LED_FB_OPEN)
  Output closeFB(PIN_PB4);   // 主流路弁閉状態表示用LED (LED_FB_CLOSE)
  Output purgeFB(PIN_PH1);   // パージ弁状態表示用LED (LED_FB_PURGE)

  // 制御タスクが正常に動作しているかを示すランプ
  Output statusLamp(PIN_PK4); // LED_TASK

  // 手動スイッチ入力に対する処理を行うタスク
  void handleManualTask();
  // 起動時のLED全点灯（クリスマスツリーテスト）開始
  void setChristmasTreeStart();
  // LED全点灯テスト終了
  void setChristmasTreeStop();
} // namespace control

namespace error
{
  // エラー状態を示すランプ
  Output statusLamp(PIN_PK6); // LED_ERROR
} // namespace error

namespace solenoid
{
  // 電磁弁の電圧や状態を監視するADC
  SolenoidMonitor monitor(PIN_PC5); // CS_ADC

  // チャタリング防止（デバウンス）用設定
  // 5Hzの監視周期で5回連続同じ状態を検出した場合に状態を確定する (約1秒)
  constexpr uint8_t FAULT_CONFIRM_COUNT = 5;

  struct Debouncer
  {
    SolenoidMonitor::Status lastRawStatus = SolenoidMonitor::Status::OFF;
    SolenoidMonitor::Status confirmedStatus = SolenoidMonitor::Status::OFF;
    uint8_t count = 0;

    /// @brief 状態を更新し、デバウンス済みの確定ステータスを返す
    SolenoidMonitor::Status update(SolenoidMonitor::Status newStatus)
    {
      if (newStatus == lastRawStatus)
      {
        count++;
      }
      else
      {
        lastRawStatus = newStatus;
        count = 1;
      }

      if (count >= FAULT_CONFIRM_COUNT)
      {
        confirmedStatus = newStatus;
      }
      return confirmedStatus;
    }

    /// @brief デバウンサの状態を初期状態にリセットする
    void reset()
    {
      lastRawStatus = SolenoidMonitor::Status::OFF;
      confirmedStatus = SolenoidMonitor::Status::OFF;
      count = 0;
    }
  };

  // 各電磁弁用のデバウンサインスタンス
  Debouncer fillDebouncer;
  Debouncer dumpDebouncer;
  Debouncer oxygenDebouncer;
  Debouncer purgeDebouncer;

  // 特定の電磁弁の電圧・状態をチェックしシリアル出力する関数
  void checkSolenoid(SolenoidMonitor::Solenoid solenoid, const char *name);
  // すべての電磁弁の状態を監視・判定するタスク
  void measureTask();
} // namespace solenoid

namespace n2o
{
  // N2O圧力値を表示する7セグメントLEDディスプレイ
  TM1637 tm1637(PIN_PK0, PIN_PK1); // 7SEG_CLK, 7SEG_DIO

  // 亜酸化窒素の圧力を読み取るための高精度ADC (ADS1115)のインスタンス
  Lib_ADS1115 ads1115;

  // 圧力センサ (4-20mA電流出力型) の読み取りインスタンス
  // 回路図に合わせてシャント抵抗を100Ωに変更、チャンネル0を使用
  VESIM10 vesim10(&ads1115, 0, 100.0, 10.0);

  // --- ゼロ点校正の安全設定 ---
  // 大気圧下において許容される生電流の範囲 (mA)
  // この範囲外の場合は、圧力が残っているかセンサ異常として校正をブロックします。
  const float CALIB_SAFE_CURRENT_MIN = 3.8;
  const float CALIB_SAFE_CURRENT_MAX = 4.2;
  // ---------------------------

  // 現在の圧力値 (MPa)
  float pressure_MPa = 0.0;

  // 圧力を計測し7セグに表示するタスク
  void measureTask();
  // センサからのデータをサンプリングするタスク
  void samplingTask();
} // namespace n2o

namespace umbilical
{
  // アンビリカル（地上とロケットを繋ぐケーブル）経由の信号出力
  Output flightMode(PIN_PL7); // フライトモード状態出力
  Output valveMode(PIN_PL6);  // バルブモード状態出力
} // namespace umbilical

namespace communication
{
  // 通信パケットの種類を定義
  enum class Packet : uint8_t
  {
    CONTROL_SYNC,              // ランチコントローラーからの制御コマンド同期（電磁弁開閉など）
    FEEDBACK_SYNC,             // サテライトコントローラーからのフィードバック（電磁弁の実際の状態など）
    PRESSURE_SYNC,             // 算出された圧力値(MPa)の同期
    COM_CHECK_L_TO_S,          // ランチコントローラーからサテライトコントローラーへの生存確認（通信チェック）
    COM_CHECK_S_TO_L,          // サテライトコントローラーからランチコントローラーへの生存確認（通信チェック）
    SENSOR_CONFIG_SYNC,        // センサの基本設定（フルスケールなど）の同期
    SENSOR_DUMMY_CURRENT_SYNC, // シミュレーション用のダミー電流値同期
    SENSOR_CALIB_COEFF_SYNC,   // 校正係数(a, b)同期用
    SENSOR_ZERO_CALIB_REQ,     // ゼロ点校正実行要求用
    SENSOR_CURRENT_SYNC,       // 生の電流値(mA)同期用
    LIMIT_SWITCH_SYNC,         // SatelliteNode からのリミットスイッチ状態同期 (bit5=ch5 など)
  };

  // RS485の送信許可ピン (HIGHで送信有効)
  Output sendEnableControl(PIN_PC3); // CTL_RS485_DERE
  // 通信アクセスランプ
  Output accessLamp(PIN_PC4); // LED_RS485_ACCESS

  // ランチコントローラから受信した最新の制御状態
  uint8_t syncState;
  // 最後に通信を受信した時刻 (タイムアウト判定用)
  unsigned long preReceivedTime = 0;
  // 通信タイムアウト時間 (ミリ秒)
  const long timeout = 5000;

  // SatelliteNode から受信した最新のリミットスイッチ状態
  // bit0=ch0, bit1=ch1, ..., bit5=ch5 (SatelliteNode::ioexp::remapSwitchBits の出力と同じ形式)
  uint8_t limitSwitchState = 0;

  /// @brief ch5 リミットスイッチが押されているか判定するヘルパー
  /// @return true: ch5が押されている（bit5=1）, false: 押されていない
  inline bool isCh5Pressed() { return (limitSwitchState >> 5) & 0x01; }

  // RS485の送信を有効化する関数
  void enableOutput();
  // RS485の送信を無効化する関数
  void disableOutput();

  // フィードバック状態を送信する
  void sendFeedbackSync();
  // 圧力値を送信する
  void sendPressureSync();
  // 生の電流値を送信する
  void sendCurrentSync();
  // 通信チェック(生存確認)を送信する
  void sendComCheck();
  // リミットスイッチ状態をランチコントローラーへ転送する
  void sendLimitSwitchSync();

  // 各種パケット受信時のコールバック関数群
  void onControlSyncReceived(uint8_t state);
  void onComCheckReceived();
  void onComCheckFailed();
  void onSensorConfigReceived(float fullScale_MPa);
  void onSensorDummyCurrentReceived(float dummyCurrent_mA);
  void onSensorCalibCoeffReceived(float a, float b);
  void onSensorZeroCalibReqReceived();
  // SatelliteNode からリミットスイッチ状態を受信した際のコールバック
  void onLimitSwitchSyncReceived(uint8_t state);

  // 通信状態が正常であることを示すランプ
  Output statusLamp(PIN_PK5); // LED_COM
} // namespace communication

/// @brief 送信を有効にする
void communication::enableOutput()
{
  communication::sendEnableControl.on();
  communication::accessLamp.pulse(50);
}

/// @brief 送信を無効にする
void communication::disableOutput() { communication::sendEnableControl.off(); }

/// @brief 電源状態（電圧・電流・温度）を定期的に測定し、異常があればエラー表示を行うタスク
void power::measureTask()
{
  // 入力電圧が電磁弁の動作下限（約10.5V）を下回っているかチェック
  bool isLowVoltage = power::input.getVoltage_V() < 10.5;
  // teleplot 用: 入力電圧のシリアル出力
  Serial.print(">inputVoltage_V:");
  Serial.println(input.getVoltage_V());

  // 入力電流が許容値（5.0A）を超過（過電流）しているかチェック
  bool isOverloadedInput = power::input.getAmpere_A() > 5.0;
  // teleplot 用: 入力電流のシリアル出力
  Serial.print(">inputAmpere_A:");
  Serial.println(input.getAmpere_A());

  // 12V系の消費電流が許容値（3.0A）を超過しているかチェック
  bool isOverloadedBus = power::bus12.getAmpere_A() > 3.0;
  // teleplot 用: 12Vバス電流のシリアル出力
  Serial.print(">bus12Ampere_A:");
  Serial.println(bus12.getAmpere_A());

  // 内部の3端子レギュレーター温度が100℃（異常加熱）を超えているかチェック
  bool isOverheated = power::thermal.getTemperature_degC() > 100.0;
  // teleplot 用: 3端子レギュレーター温度のシリアル出力
  Serial.print(">regulatorTemperature_degC:");
  Serial.println(thermal.getTemperature_degC());

  // 低電圧状態なら警告ランプを点灯させる
  power::lowVoltageLamp.set(isLowVoltage);

  // いずれかの異常（過電流・過熱）が検出された場合はエラーランプを点灯させる
  if (isOverloadedInput || isOverloadedBus || isOverheated)
  {
    // HACK: エラー発生時の処理。必要ならここで出力を遮断するなどの安全処理を追加
    error::statusLamp.on();
  }
}

/// @brief 電磁弁の動作状態を監視・判定し、フィードバックLEDに反映させるタスク
void solenoid::measureTask()
{
  // 仮の振る舞い（現在はコメントアウト）
  // bool isArmed = control::safetyArmed.isManualRaised();

  // (正常・故障の判定用ロジックのバックアップはコメントアウト済)

  // シリアルモニタへの表示とデバッグのため、各電磁弁の電圧とステータスを読み出して出力
  checkSolenoid(SolenoidMonitor::Solenoid::FILL, "FILL");
  checkSolenoid(SolenoidMonitor::Solenoid::DUMP, "DUMP");
  checkSolenoid(SolenoidMonitor::Solenoid::OXYGEN, "OXYGEN");
  checkSolenoid(SolenoidMonitor::Solenoid::PURGE, "PURGE");

  // SolenoidMonitor を使って、ADCの読み取り値から電磁弁の状態（正常・断線・短絡など）を判定
  SolenoidMonitor::Status rawFillStatus =
      monitor.getStatus(SolenoidMonitor::Solenoid::FILL);
  SolenoidMonitor::Status rawDumpStatus =
      monitor.getStatus(SolenoidMonitor::Solenoid::DUMP);
  SolenoidMonitor::Status rawOxygenStatus =
      monitor.getStatus(SolenoidMonitor::Solenoid::OXYGEN);
  SolenoidMonitor::Status rawPurgeStatus =
      monitor.getStatus(SolenoidMonitor::Solenoid::PURGE);

  // 安全装置（Armedスイッチ）がONになっていない場合は、フィードバック状態をすべてOFFにして終了
  if (!control::safetyArmed.isManualRaised())
  {
    // 安全装置が解除されているときは、デバウンサ状態もリセットしておく
    fillDebouncer.reset();
    dumpDebouncer.reset();
    oxygenDebouncer.reset();
    purgeDebouncer.reset();

    control::fillFB.off();
    control::dumpFB.off();
    control::oxygenFB.off();
    control::purgeFB.off();
    return;
  }

  // デバウンスを適用して確定ステータスを取得
  SolenoidMonitor::Status fillStatus = fillDebouncer.update(rawFillStatus);
  SolenoidMonitor::Status dumpStatus = dumpDebouncer.update(rawDumpStatus);
  SolenoidMonitor::Status oxygenStatus = oxygenDebouncer.update(rawOxygenStatus);
  SolenoidMonitor::Status purgeStatus = purgeDebouncer.update(rawPurgeStatus);

  // 以下の主要弁と点火装置については、単純に出力指示と同じ状態をフィードバックとして反映する
  control::openFB.set(control::open.isHigh());
  control::closeFB.set(control::close.isHigh());
  control::igniterFB.set(control::igniter.isHigh());

  // FILL弁の状態に応じて、パネルのLED表示を切り替える
  switch (fillStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE: // 断線などの開故障 -> LEDを点滅（エラー）
    control::fillFB.toggle();
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE: // 短絡などの閉故障 -> LED消灯
    control::fillFB.off();
    break;
  case SolenoidMonitor::Status::OFF: // 正常なOFF状態 -> LED消灯
    control::fillFB.off();
    break;
  case SolenoidMonitor::Status::ON: // 正常なON状態 -> LED点灯
    control::fillFB.on();
    break;
  }

  // DUMP弁の状態に応じてLED表示を切り替える
  switch (dumpStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE:
    control::dumpFB.toggle();
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE:
    control::dumpFB.off();
    break;
  case SolenoidMonitor::Status::OFF:
    control::dumpFB.off();
    break;
  case SolenoidMonitor::Status::ON:
    control::dumpFB.on();
    break;
  }

  // OXYGEN弁の状態に応じてLED表示を切り替える
  switch (oxygenStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE:
    control::oxygenFB.toggle();
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE:
    control::oxygenFB.off();
    break;
  case SolenoidMonitor::Status::OFF:
    control::oxygenFB.off();
    break;
  case SolenoidMonitor::Status::ON:
    control::oxygenFB.on();
    break;
  }

  // PURGE弁の状態に応じてLED表示を切り替える
  switch (purgeStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE:
    control::purgeFB.toggle();
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE:
    control::purgeFB.off();
    break;
  case SolenoidMonitor::Status::OFF:
    control::purgeFB.off();
    break;
  case SolenoidMonitor::Status::ON:
    control::purgeFB.on();
    break;
  }
}

/// @brief 指定した電磁弁の電圧とステータスを読み取り、シリアルモニタに出力するヘルパー関数
void solenoid::checkSolenoid(SolenoidMonitor::Solenoid solenoid, const char *name)
{
  // 電圧値(mV)の取得
  uint16_t voltage = monitor.getVoltage_mV(solenoid);

  // 判定状態の取得 (ON, OFF, 断線, 短絡)
  SolenoidMonitor::Status status = monitor.getStatus(solenoid);

  // ステータスを文字列(const char*)に変換
  const char *statusStr;
  switch (status)
  {
  case SolenoidMonitor::Status::ON:
    statusStr = "ON";
    break;
  case SolenoidMonitor::Status::OFF:
    statusStr = "OFF";
    break;
  case SolenoidMonitor::Status::OPEN_FAILURE:
    statusStr = "OPEN FAILURE";
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE:
    statusStr = "CLOSE FAILURE";
    break;
  }
  // 結果をシリアル経由で出力（デバッグ用）
  Serial.print(name);
  Serial.print(": Voltage=");
  Serial.print(voltage);
  Serial.print("mV, Status=");
  Serial.println(statusStr);
}

/// @brief N2O(亜酸化窒素)の圧力を算出し、7セグメントLEDに表示するタスク
void n2o::measureTask()
{
  // フィルタリング後の電流値(mA)を取得
  float current_mA = n2o::vesim10.read();

  Serial.print("N2OsensorCurrent_mA: ");
  Serial.println(current_mA);

  // 電流が0.1mA未満の場合はセンサ未接続などと判断し、ディスプレイを消去
  if (current_mA < 0.1)
  {
    n2o::tm1637.clearDisplay();
  }
  else
  {
    // 現在の電流値から圧力値(MPa)を換算
    n2o::pressure_MPa = n2o::vesim10.getPressure_MPa();
    // 算出した圧力を7セグメントLEDに表示（絶対値で表示）
    n2o::tm1637.displayNumber(abs(n2o::pressure_MPa));
  }
}

/// @brief N2O圧力センサ(ADC)からの値を高頻度でサンプリングし、内部のバッファに蓄積するタスク
void n2o::samplingTask() { n2o::vesim10.sample(); }

/// @brief 現在の電磁弁の実際の状態（フィードバック）をパッキングし、RS485経由でランチコントローラー側に送信する
void communication::sendFeedbackSync()
{
  // 各LED状態からビットフラグを生成し、1バイト(uint8_t)にまとめる
  uint8_t state =
      (control::shiftFB.isHigh() << 0) | (control::fillFB.isHigh() << 1) |
      (control::dumpFB.isHigh() << 2) | (control::oxygenFB.isHigh() << 3) |
      (control::igniterFB.isHigh() << 4) | (control::openFB.isHigh() << 5) |
      (control::closeFB.isHigh() << 6) | (control::purgeFB.isHigh() << 7);

  communication::enableOutput(); // DE=HIGHにして送信モードへ切り替え

  // FEEDBACK_SYNC パケットとして状態データをシリアル1(RS485)へ送信
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::FEEDBACK_SYNC), state);

  // 送信バッファのデータがすべて物理的に配線に送り出されるのを待つ
  Serial1.flush();

  communication::disableOutput(); // 送信完了後、DE=LOWにして受信モードへ戻す
}

/// @brief 算出されたN2O圧力値(MPa)をRS485経由でランチコントローラー側に送信する
void communication::sendPressureSync()
{
  communication::enableOutput();

  // PRESSURE_SYNC パケットとして圧力値(float)を送信
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::PRESSURE_SYNC), n2o::pressure_MPa);
  Serial1.flush();

  communication::disableOutput();
}

/// @brief センサから取得した生の電流値(mA)をRS485経由でランチコントローラー側に送信する
void communication::sendCurrentSync()
{
  float current_mA = n2o::vesim10.getCurrent_mA(); // 生の電流値を取得
  communication::enableOutput();
  // SENSOR_CURRENT_SYNC パケットとして電流値を送信
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CURRENT_SYNC), current_mA);
  Serial1.flush();
  communication::disableOutput();
}

/// @brief SatelliteNode から受信したリミットスイッチ状態をランチコントローラーへ転送する
/// SatelliteNode → SatelliteController → LaunchController という中継パスです。
/// ランチコントローラー側がこのデータを使い、ch5未押下の場合にシーケンスを止めます。
void communication::sendLimitSwitchSync()
{
  communication::enableOutput();
  // LIMIT_SWITCH_SYNC パケットとして、SatelliteNode から受信した状態をそのまま転送
  MsgPacketizer::send(Serial1,
                      static_cast<uint8_t>(communication::Packet::LIMIT_SWITCH_SYNC),
                      communication::limitSwitchState);
  Serial1.flush();
  communication::disableOutput();
}

/// @brief SatelliteNode からリミットスイッチ状態パケットを受信した際のコールバック
/// 受信した状態を変数に保存し、次の sendLimitSwitchSync() でランチ側へ転送します。
void communication::onLimitSwitchSyncReceived(uint8_t state)
{
  communication::limitSwitchState = state;

  // teleplot / デバッグ用: ch5 の状態をシリアルで確認できるようにする
  Serial.print(">limitSwitch_ch5:");
  Serial.println(communication::isCh5Pressed() ? 1 : 0);
}

/// @brief サテライトからランチに対して生存確認（ハートビート）を送信する
void communication::sendComCheck()
{
  communication::enableOutput();
  // COM_CHECK_S_TO_L パケット（データなし）を送信
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_S_TO_L));
  Serial1.flush();
  communication::disableOutput();
}

/// @brief ランチ側から制御コマンド（開閉指示など）の同期パケットを受信した際のコールバック
void communication::onControlSyncReceived(uint8_t state)
{
  // 安全装置（Armedスイッチ）がONになっているか確認
  bool isArmed = control::safetyArmed.isManualRaised();

  // 受信した制御状態を保存
  communication::syncState = state;
  // ★受信成功したので通信ランプを点灯し、エラーランプを消灯
  communication::statusLamp.on();
  error::statusLamp.off();

  // 受信したビットマップデータ(state)を展開し、さらにArmed状態なら電磁弁等を作動させる
  // control::shift.set(state & (1 << 0) && isArmed); // シフト弁 (現在は無効化)
  control::fill.set(state & (1 << 1) && isArmed);
  control::dump.set(state & (1 << 2) && isArmed);
  control::oxygen.set(state & (1 << 3) && isArmed);
  control::igniter.set(state & (1 << 4) && isArmed);
  control::open.set(state & (1 << 5) && isArmed);
  control::close.set(state & (1 << 6) && isArmed);
  control::purge.set(state & (1 << 7) && isArmed);

  // 【安全策】もし主弁がCLOSE(閉指令)なのに、DUMP(排出)もONになっていた場合、背反するためDUMP側を強制OFF
  if ((control::dump.isHigh()) && (control::close.isHigh()))
  {
    control::dump.off();
  }
}

/// @brief ランチ側からの生存確認（ハートビート）を受信した際のコールバック
void communication::onComCheckReceived()
{
  communication::statusLamp.on();            // 通信ランプ点灯
  error::statusLamp.off();                   // エラーランプ消灯
  communication::preReceivedTime = millis(); // 最終通信時刻を更新（タイムアウト判定用）
}

/// @brief ランチ側からセンサのフルスケールレンジ設定を受信した際のコールバック
void communication::onSensorConfigReceived(float fullScale_MPa)
{
  n2o::vesim10.setFullScale(fullScale_MPa); // VESIM10にフルスケールを設定
}

/// @brief ランチ側からシミュレーション用のダミー電流値を受信した際のコールバック
void communication::onSensorDummyCurrentReceived(float dummyCurrent_mA)
{
  // 0.1mA 未満ならダミーモード解除、それ以上なら指定された電流値でシミュレーション開始
  if (dummyCurrent_mA < 0.1)
  {
    n2o::vesim10.disableDummy();
  }
  else
  {
    n2o::vesim10.setDummyCurrent(dummyCurrent_mA);
  }
}

/**
 * @brief センサ校正係数の受信ハンドラ
 * 実測に基づく一次式の傾き(a)と切片(b)を直接設定します。
 */
void communication::onSensorCalibCoeffReceived(float a, float b)
{
  n2o::vesim10.setCalibration(a, b);
}

/**
 * @brief ゼロ点校正実行リクエストの受信ハンドラ
 * 現在の圧力（大気圧下を想定）を 0MPa としてオフセットを再計算します。
 */
void communication::onSensorZeroCalibReqReceived()
{
  // 安全チェック: 生の電流値が大気圧相当（約4mA）であることを確認
  float current = n2o::vesim10.getCurrent_mA();

  // 電流が安全な範囲内であれば校正を実行
  if (current >= n2o::CALIB_SAFE_CURRENT_MIN && current <= n2o::CALIB_SAFE_CURRENT_MAX)
  {
    Serial.println(">>> Zero-Point Calibration: Started.");
    n2o::vesim10.calibrateBlocking(10); // 10回サンプリングして校正
    Serial.println(">>> Zero-Point Calibration: Successfully completed.");
  }
  else
  {
    // 安全範囲外なら圧力が残っているかセンサ異常として校正をブロックする
    Serial.print(">>> CALIBRATION BLOCKED: Sensor current out of safe range (");
    Serial.print(current);
    Serial.println(" mA). Pressure may be present.");
  }
}

/// @brief ランチ側との通信が途絶（タイムアウト）したかをチェックし、フェールセーフを発動するタスク
void communication::onComCheckFailed()
{
  // 最後に通信を受信した時刻から規定時間（timeout）以上経過していた場合
  if (millis() - communication::preReceivedTime > communication::timeout)
  {
    communication::statusLamp.off(); // 通信ランプ消灯
    error::statusLamp.on();          // エラーランプ点灯

    // 通信失敗時は安全のため全出力を強制OFF
    control::dump.off();
    control::fill.off();
    control::oxygen.off();
    control::igniter.off();
    control::open.off();
    control::close.off();
    control::purge.off();

    // control::dump.set(communication::syncState & 0);
    // control::fill.set(communication::syncState & 0);
    // control::oxygen.set(communication::syncState & 0);
    // control::igniter.set(communication::syncState & 0);
    // control::open.set(communication::syncState & 0);
    // control::close.set(communication::syncState & 0);
    // control::purge.set(communication::syncState & 0);
  }
}

/// @brief 物理スイッチ（キルスイッチや手動パージなど）の入力を処理するタスク
void control::handleManualTask()
{
  // タスク稼働中であることを示すランプを点滅させる
  control::statusLamp.pulse(50);

  // キルスイッチ（緊急停止ボタン）が押されている場合の処理
  if (power::killButton.isHigh())
  {
    // 電源ランプを点滅させて緊急停止状態を警告する
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
    // メインの電源供給（ロードスイッチ）を物理的に遮断する
    power::loadSwitch.off();
  }

  // セーフティスイッチの読み取り
  control::safetyArmed.setManual();
  // Armed状態（安全装置解除）でなければ、手動操作は行えないので終了
  if (!control::safetyArmed.isManualRaised())
  {
    return;
  }

  // 手動操作チェック用スイッチ（パージ用）の読み取り
  control::check.setManual();
  if (control::check.isManualRaised())
  {
    control::purge.on(); // 手動でパージ弁を開く
  }
  // (旧パージスイッチのロジックはコメントアウト済)

  // アンビリカル（地上と接続されているケーブル）への状態出力
  umbilical::flightMode.set(control::igniter.isHigh());                         // 点火中ならフライトモードとして出力
  umbilical::valveMode.set(control::open.isHigh() && !control::close.isHigh()); // 開弁状態ならバルブモードとして出力
}

/// @brief 起動時にすべてのLEDやセグメントを点灯させるテスト（クリスマスツリーテスト）を開始
void control::setChristmasTreeStart()
{
  n2o::tm1637.displayNumber(8.8); // 7セグの全セグメントを点灯
  error::statusLamp.setTestOn();  // 各種ランプをテスト点灯
  power::lowVoltageLamp.setTestOn();
  control::statusLamp.setTestOn();
  communication::accessLamp.setTestOn();
  communication::statusLamp.setTestOn();
  control::safetyArmed.setTestOn();
  control::shiftFB.setTestOn();
  control::fillFB.setTestOn();
  control::dumpFB.setTestOn();
  control::oxygenFB.setTestOn();
  control::igniterFB.setTestOn();
  control::openFB.setTestOn();
  control::closeFB.setTestOn();
  control::purgeFB.setTestOn();
  control::check.setTestOn();
}

/// @brief LED全点灯テスト（クリスマスツリーテスト）を終了し、通常表示に戻す
void control::setChristmasTreeStop()
{
  n2o::tm1637.clearDisplay();     // 7セグを消灯
  error::statusLamp.setTestOff(); // 各種ランプのテスト点灯を解除
  power::lowVoltageLamp.setTestOff();
  control::statusLamp.setTestOff();
  communication::accessLamp.setTestOff();
  communication::statusLamp.setTestOff();
  control::safetyArmed.setTestOff();
  control::shiftFB.setTestOff();
  control::fillFB.setTestOff();
  control::dumpFB.setTestOff();
  control::oxygenFB.setTestOff();
  control::igniterFB.setTestOff();
  control::openFB.setTestOff();
  control::closeFB.setTestOff();
  control::purgeFB.setTestOff();
  control::check.setTestOff();
}

/// @brief 初回起動時に実行される初期化処理
void setup()
{
  power::loadSwitch.on(); // 電源出力ラインをONにする
  power::powerLamp.on();  // 電源ランプを点灯させる

  // デバッグ及び外部通信用のUSBシリアル通信を初期化
  Serial.begin(115200);

  // 起動時のアスキーアート（ロゴ）を表示
  Serial.println("   __ __    ____      ____  _____ ____  ___ ");
  Serial.println("  / // /__ / / /__   /_  / / ___// __ \\/ _ \\");
  Serial.println(" / _  / -_) / / _ \\   / /  \\__ \\/ /_/ / ___/");
  Serial.println("/_//_/\\__/_/_/\\___/ /_/  /____/\\____/_/    ");
  Serial.println("");
  Serial.println("  ____  _____ _   _          __        ____ ____  _____ ");
  Serial.println(" / ___|| ____| \\ | |        / /_      / ___/ ___|| ____|");
  Serial.println("| |  _ |  _| |  \\| |_____  | '_ \\    | |  _\\___ \\|  _|  ");
  Serial.println("| |_| || |___| |\\  |_____| | (_) |   | |_| |___) | |___ ");
  Serial.println(" \\____||_____|_| \\_|        \\___/     \\____|____/|_____|");
  Serial.println("");

  // ランチコントローラーと通信するRS485用ハードウェアシリアルの初期化
  Serial1.begin(115200);
  // 初期状態でタイムアウトにならないよう、過去の時刻をセットしておく
  communication::preReceivedTime = millis();

  // SPI通信（MCP3208のADC用）の初期化
  SPI.begin();
  // 電磁弁監視用の分圧抵抗の比率を設定 (5.6kΩ と 3.3kΩ)
  solenoid::monitor.setDividerResistance(5600, 3300);

  // 7セグメントLED（TM1637）の初期化
  n2o::tm1637.initialize();

  // I2C通信と電力モニタ（INA219）の初期化
  Wire.begin();
  power::input.begin();
  power::bus12.begin();

  // N2O圧力センサ読み取り用高精度ADC（ADS1115）の初期化
  if (n2o::ads1115.init())
  {
    n2o::ads1115.setVoltageRange(2); // 4-20mAが100Ωに流れた時の最大電圧は2.0Vなので、±2.048Vの範囲が最適
  }
  else
  {
    Serial.println("Error: ADS1115 Initialization Failed.");
  }

  // (自動ゼロ点校正は安全のため現在はコメントアウト。地上からのコマンドで実行する)
  // n2o::vesim10.calibrateBlocking(10);

  // TaskManagerへ各種タスクを登録し、実行頻度（Hz）を設定する
  Tasks.add(&power::measureTask)->startFps(5);              // 5Hzで電源状態を監視
  Tasks.add(&solenoid::measureTask)->startFps(5);           // 5Hzで電磁弁の状態を監視
  Tasks.add(&n2o::samplingTask)->startFps(20);              // 20Hzで圧力センサをサンプリング
  Tasks.add(&n2o::measureTask)->startFps(2);                // 2Hzで圧力を計算・表示
  Tasks.add(&control::handleManualTask)->startFps(5);       // 5Hzで手動スイッチ入力をチェック
  Tasks.add(&communication::sendFeedbackSync)->startFps(5); // 5Hzで電磁弁状態をRS485で送信
  Tasks.add(&communication::sendPressureSync)->startFps(2); // 2Hzで圧力値をRS485で送信
  Tasks.add(&communication::sendCurrentSync)->startFps(2);  // 2Hzでセンサ電流値をRS485で送信
  Tasks.add(&communication::sendComCheck)->startFps(2);          // 2Hzで生存確認パケットを送信
  Tasks.add(&communication::onComCheckFailed)->startFps(2);      // 2Hzで通信途絶の監視を行う
  // SatelliteNode から受信したリミットスイッチ状態をランチ側に転送するタスク
  // SatelliteNode が 10Hz で送信しているため、こちらも 10Hz で転送する
  Tasks.add(&communication::sendLimitSwitchSync)->startFps(10);

  // MsgPacketizer（パケット通信ライブラリ）の受信設定。対応するパケットが届いた際のコールバックを登録
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::CONTROL_SYNC), &communication::onControlSyncReceived);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_S), &communication::onComCheckReceived);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CONFIG_SYNC), &communication::onSensorConfigReceived);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_DUMMY_CURRENT_SYNC), &communication::onSensorDummyCurrentReceived);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CALIB_COEFF_SYNC), &communication::onSensorCalibCoeffReceived);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_ZERO_CALIB_REQ), &communication::onSensorZeroCalibReqReceived);
  // SatelliteNode からのリミットスイッチパケットを受信するコールバックを登録
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::LIMIT_SWITCH_SYNC), &communication::onLimitSwitchSyncReceived);

  // 起動時のLED全点灯テスト（クリスマスツリーテスト）を開始
  control::setChristmasTreeStart();
  // 3秒後にテストを終了するタスクを登録
  Tasks.add(&control::setChristmasTreeStop)->startOnceAfterSec(3.0);
}

/// @brief Arduinoのメインループ。可能な限り高速で回り続ける
void loop()
{
  // 受信バッファにあるパケットを解析し、該当するコールバックを呼び出す
  MsgPacketizer::parse();
  // 登録されたタスクの実行タイミングを管理・実行する
  Tasks.update();

  // 通信アクセスランプと制御状態ランプの更新処理（点滅管理）
  communication::accessLamp.update(); // RS485用
  control::statusLamp.update();       // タスク稼働確認用

  Serial.println(communication::preReceivedTime);
}
