#include <DFPlayer_Mini_Mp3.h>

void setup() {
  Serial.begin(9600);
  mp3_set_serial(Serial);
  mp3_set_volume(100);

  mp3_play(1);
}

void loop() {
}