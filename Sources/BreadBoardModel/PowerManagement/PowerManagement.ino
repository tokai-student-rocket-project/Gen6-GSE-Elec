void setup() {
  // RUNスイッチ
  pinMode(A5, OUTPUT);
  digitalWrite(A5, HIGH);

  // KILLスイッチ
  pinMode(14, INPUT);

  // LED TASK
  pinMode(A12, OUTPUT);
}

void loop() {
  // KILL処理
  if (digitalRead(14)) {
    digitalWrite(A5, LOW);
  }

  // LED TASK 点滅
  digitalWrite(A12, !digitalRead(A12));
  delay(100);
}