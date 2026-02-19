/*
  IMPORTANT NOTE FOR ARDUINO USERS
  ================================

  The language used in Arduino sketches is a subset of C/C++.
  However, in Embeetle you should use plain C/C++, which means
  that:

    - Functions should have a function prototype, usually
      declared at the top of the file.

    - Include statements are important to use functions,
      variables and classes from other files.

*/

#include <Arduino.h>

/*
  Test Serial Connection
  
  Start a serial connection and send "Hello World"
  immediately when established. Then, send the string
  "embeetle" repeatedly, while blinking an LED.

  Most Arduinos have an on-board LED you can control. On
  the UNO, MEGA and ZERO it is attached to digital pin 13,
  on MKR1000 on pin 6. LED_BUILTIN is set to the correct
  LED pin independent of which board is used. If you want
  to know what pin the on-board LED is connected to on your
  Arduino model, check the Technical Specs of your board at:
  https://www.arduino.cc/en/Main/Products

  This example code is in the public domain.
*/

// the setup function runs once
// when you press reset or power the board


void setup();

void loop();

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  // set up Serial library at 9600 bps
  Serial.begin(9600);
  Serial.println("Hello World");
  Serial.println("This is an Arduino Due board");
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  Serial.println("embeetle");
}
