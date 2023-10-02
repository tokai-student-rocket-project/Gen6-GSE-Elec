#include <Arduino.h>
#line 1 "/Users/waiwai/Projects/Gen6-GSE-Elec/Sources/BreadBoardModel/PowerManagement/PowerManagement.ino"
#line 1 "/Users/waiwai/Projects/Gen6-GSE-Elec/Sources/BreadBoardModel/PowerManagement/PowerManagement.ino"
void setup();
#line 10 "/Users/waiwai/Projects/Gen6-GSE-Elec/Sources/BreadBoardModel/PowerManagement/PowerManagement.ino"
void loop();
#line 1 "/Users/waiwai/Projects/Gen6-GSE-Elec/Sources/BreadBoardModel/PowerManagement/PowerManagement.ino"
void setup() {
  pinMode(A5, OUTPUT);
  digitalWrite(A5, HIGH);

  pinMode(14, INPUT);

  pinMode(A12, OUTPUT);
}

void loop() {
  if (digitalRead(14)) {
    digitalWrite(A5, LOW);
  }

  digitalWrite(A12, !digitalRead(A12));
  delay(100);
}
