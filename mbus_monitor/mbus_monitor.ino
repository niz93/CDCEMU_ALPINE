#define MBUS_IN_PIN 10 // Input port.
#define MBUS_OUT_PIN 12 // Output port.

#define LED_OUT_PIN 7 // Output port.

#define MBUS_IN_PIN_INTERRUPT 2

#define DEFAULT_ZERO_TIME 600
#define DEFAULT_ONE_TIME  1900
#define DEFAULT_TOLERANCE 450

#define MIN_ZERO_TIME (DEFAULT_ZERO_TIME - DEFAULT_TOLERANCE)
#define MAX_ZERO_TIME (DEFAULT_ZERO_TIME + DEFAULT_TOLERANCE)
#define MIN_ONE_TIME (DEFAULT_ONE_TIME - DEFAULT_TOLERANCE)
#define MAX_ONE_TIME (DEFAULT_ONE_TIME + DEFAULT_TOLERANCE)

volatile byte led_state = LOW;

struct MbusReceiveData {
  byte state;
  byte data_state;
  uint8_t num_bits;
  unsigned long timer_us;
  uint8_t four_bits;
  uint8_t num_chars;
  char message[50];
};
volatile MbusReceiveData receive_data = {0, 0, 0, 0, 0, 0};

void setup() {
  Serial.begin(115200);
  Serial.write("init\n");

  pinMode(MBUS_IN_PIN_INTERRUPT, INPUT);
  pinMode(LED_OUT_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(MBUS_IN_PIN_INTERRUPT), handle_mbus, CHANGE);
}

void loop() {
  digitalWrite(LED_OUT_PIN, led_state);
}

void handle_mbus() {
  led_state = !led_state;

  // Move this to a timer routine!
  if (/*receive_data.state > 0 &&*/ (micros() - receive_data.timer_us) > 4000) {
    // Reset the state as the timeout happened.
    receive_data.state = 0;
    receive_data.num_bits = 0;

    // Cut the last char as it's a checksum we don't care about.
    receive_data.message[receive_data.num_chars - 1] = '\0';

    if (receive_data.message[0] == '1') {
      Serial.print("HU: ");
    } else if (receive_data.message[0] == '9')  {
      Serial.print("CDC: ");
    } else {
      Serial.print("Unknown: ");
    }

    Serial.println((char*)receive_data.message);

    receive_data.num_chars = 0;
  }

  if (receive_data.state == 0 && digitalRead(MBUS_IN_PIN_INTERRUPT) == HIGH) {
    // A bit has just finished, nothing to do here.
    return;
  }

  if (digitalRead(MBUS_IN_PIN_INTERRUPT) == LOW) {
    receive_data.timer_us = micros();
    receive_data.state = 1;
  } else {
    // High after low - a bit has just finished.
    unsigned long diff_us = micros() - receive_data.timer_us;
    receive_data.state = 2;

    if (diff_us < MIN_ZERO_TIME) {
      // Pulse too short.
      receive_data.data_state = 1;
      Serial.println("\r\nerr1");
    } else if (diff_us <= MAX_ZERO_TIME) {
      receive_data.four_bits *= 2;
    } else if (diff_us < MIN_ONE_TIME) {
      // Incorrect pulse - inbetween zero and one.
      receive_data.data_state = 2;
      Serial.println("\r\nerr2");
    } else if (diff_us <= MAX_ONE_TIME) {
      receive_data.four_bits *= 2;
      receive_data.four_bits += 1;
    } else {
      receive_data.data_state = 3;
      // Pulse too long.
      Serial.println("\r\nerr3");
    }

    ++receive_data.num_bits;

    if ((receive_data.num_bits % 4) == 0) {
      //receive_data.message += String(receive_data.four_bits);
      //Serial.println(receive_data.four_bits);
      char data_string[1]; // = {0};
      sprintf(data_string,"%x", receive_data.four_bits);
      receive_data.message[receive_data.num_chars] = data_string[0];
      
      receive_data.four_bits = 0;
      receive_data.num_bits = 0;
      ++receive_data.num_chars;
    }

    receive_data.state = 0;
    receive_data.timer_us = micros();
  }
}
