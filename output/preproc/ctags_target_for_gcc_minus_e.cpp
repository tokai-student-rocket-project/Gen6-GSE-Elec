# 1 "/Users/waiwai/Projects/Gen6-GSE-Elec/Sources/BreadBoardModel/PowerManagement/PowerManagement.ino"
void setup() {
  pinMode(A5, 0x1);
  digitalWrite(A5, 0x1);

  pinMode(14, 0x0);

  pinMode(A12, 0x1);
}

void loop() {
  if (digitalRead(14)) {
    digitalWrite(A5, 0x0);
  }

  digitalWrite(A12, !digitalRead(A12));
  delay(100);
}
