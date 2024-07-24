#include <avr/pgmspace.h>
#include <TimerOne.h>

// Adapted to Teensy.
#define MBUS_IN_PIN 2 // Input port.
#define MBUS_IN_PIN_INTERRUPT MBUS_IN_PIN
#define MBUS_OUT_PIN 3 // Output port.
#define LED_OUT_PIN LED_BUILTIN // Output port.

#define DEFAULT_ZERO_TIME 600
#define DEFAULT_ONE_TIME  1900
#define DEFAULT_TOLERANCE 450

#define MIN_ZERO_TIME (DEFAULT_ZERO_TIME - DEFAULT_TOLERANCE)
#define MAX_ZERO_TIME (DEFAULT_ZERO_TIME + DEFAULT_TOLERANCE)
#define MIN_ONE_TIME (DEFAULT_ONE_TIME - DEFAULT_TOLERANCE)
#define MAX_ONE_TIME (DEFAULT_ONE_TIME + DEFAULT_TOLERANCE)

//// Adapted the table from Jörg Hohensohn (http://www.hohensohn.info/mbus/index.htm).
typedef struct
{  // one entry in the coding table
  char szMask[32];
  char szComment[32];
} tEntry;

static const tEntry aCodetable[] PROGMEM = {
  { "18", "Ping" },
  { "98", "Ping OK" },
  { "9F0000f", "Ack/Wait" }, // f0=0|1|6|7|9
  { "991ttiimmssff0f", "Preparing" }, //f0=0:normal, f0=4:repeat one, f0=8:repeat all
  { "992ttiimmssff0f", "Stopped" }, //f1=0:normal, f1=2:mix, f1=8:scan
  { "993ttiimmssff0f", "Paused" }, // f3=1: play mode, f3=2:paused mode, f3|=8: stopped
  { "994ttiimmssff0f", "Playing" },
  { "995ttiimmssff0f", "Spinup" },
  { "996ttiimmssff0f", "FF" },
  { "997ttiimmssff0f", "FR" },
  { "11101", "Play" },
  { "11102", "Pause" },
  { "11140", "Stop" },
  { "11150", "ScanStop" },
  { "11105", "Play FF start" },
  { "11109", "Play FR start" },
  { "11106", "Pause FF start" },
  { "1110A", "Pause FR start" },
  { "11181", "Play from current pos." },
  { "11182", "Pause from current pos." },
  { "1130A314", "next random" },
  { "1130B314", "previous random" },
  { "113dttff", "Select" }, // f0=1:playing, f0=2:paused, f1=4:random
  { "113dttf", "Select 7909" }, // 113d001 play, 113d002 pause, but also 1130A32?; 1130021 2nd track; 1130031 3rd track; d=0 means current
  { "11400000", "Repeat Off" },
  { "11440000", "Repeat One" },
  { "11480000", "Repeat All" },
  { "1140000", "Repeat Off 7909"},
  { "1144000", "Repeat One 7909"},
  { "1148000", "Repeat All 7909"},
  { "11408000", "Scan" },
  { "1140800", "Scan 7909" },
  { "1140100", "Scan Off 7909" },
  { "11402000", "Mix" },
  { "1140200", "Mix 7909" },
  { "1140500", "Mix Off 7909" },
  { "9A0000000000", "some powerup?" },
  { "9B0dttfff0f", "last played" }, // f0=0:done, f0=1:busy, f0=8:eject, //f1=4: repeat1, f1=8:repeat all, f2=2:mix
  { "9B8d00fff0f", "Changing Phase 4" },
  { "9B9dttfff0f", "Changing Done" }, 
  { "9BAd00f00ff", "No Magazin" },
  { "9BBd00fff0f", "Changing Phase 2" },
  { "9BCd00fff0f", "Changing Phase 3" },
  { "9BDd00fff0f", "Changing Phase 1" },
  { "9Cd01ttmmssf", "Disk Status" },
  { "9D000fffff", "some status?" },
  { "9E0000000", "some more status?" },
//  // also seen:
//  // 11191
};



volatile byte led_state = LOW;

struct MbusReceiveData {
  byte state;
  byte data_state;
  uint8_t num_bits;
  unsigned long timer_us;
  uint8_t four_bits;
  uint8_t num_chars;
  volatile bool message_ready;
  volatile char message[32];
};
volatile MbusReceiveData receive_data = {0, 0, 0, 0, 0, 0, false};

void matchAgainstCommandTable(volatile char command[]) {
  char cmd_copy[32];
  strcpy(cmd_copy, (char*)command);

  const int commandlen = strlen(cmd_copy);
  const char first_char = cmd_copy[0];

  bool found_match = false;
  char description[31];
  
  for (int i = 0; i < sizeof(aCodetable) / sizeof(*aCodetable); ++i) {
    if (commandlen != strlen_P(aCodetable[i].szMask)) {
      continue; // size mismatch
    }

    char temp[21];
    strncpy_P(temp, aCodetable[i].szMask, 20);
    
    const char* pszRun = cmd_copy; // source read pointer
    const char* pszCompare = temp; // current compare
    
    int j;
    for (j = 0; j < commandlen; ++j) { 
      // All (upper case) hex digits of the mask must match.
      if ((*pszCompare >= '0' &&  *pszCompare <= '9') || (*pszCompare >= 'A' &&  *pszCompare <= 'F')) {
        if (*pszCompare != *pszRun) {
          break; // exit the char loop
        }
      }
      pszRun++;
      pszCompare++;
    }
    if (j == commandlen) {
      // It's a match!
      strncpy_P(description, aCodetable[i].szComment, 30);
      found_match = true;
    }
  }

  if (cmd_copy[0] == '1') {
    Serial.print("HU: ");
  } else if (cmd_copy[0] == '9')  {
    Serial.print("CDC: ");
  } else {
    Serial.print("Unknown: ");
  }
  
  Serial.print(cmd_copy);
  Serial.print(" ");
  
  if (found_match) {
    Serial.println(description);
  } else {
    Serial.println(" UNKNOWN CMD");
  }

  receive_data.message_ready = false;
}

void setup() {
  Serial.begin(115200);
  Serial.write("init\n");

  pinMode(MBUS_IN_PIN_INTERRUPT, INPUT_PULLUP);
  pinMode(LED_OUT_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(MBUS_IN_PIN_INTERRUPT), handleMbus, CHANGE);

  receive_data.timer_us = micros();
  Timer1.initialize(1000);
  Timer1.attachInterrupt(checkFinished);
}

void loop() {
  digitalWrite(LED_OUT_PIN, led_state);
  noInterrupts();
  if (receive_data.message_ready) {
    matchAgainstCommandTable(receive_data.message);
  }
  interrupts();
}

void checkFinished() {
  // We should move this to an interrupt routine one day.
  if (!receive_data.message_ready && receive_data.state == 4 && (micros() - receive_data.timer_us) > 4000) {
    // Reset the state as the timeout happened.
    receive_data.state = 0;
    receive_data.num_bits = 0;

    // Cut the last char as it's a checksum we don't care about.
    receive_data.message[receive_data.num_chars - 1] = '\0';

    receive_data.message_ready = true;
    receive_data.num_chars = 0;
  }
}

void handleMbus() {
  led_state = !led_state;

  if (receive_data.state == 0 && digitalRead(MBUS_IN_PIN_INTERRUPT) == HIGH) {
    // A stray bit we haven't registered has just finished, nothing to do here.
    return;
  }

  if (digitalRead(MBUS_IN_PIN_INTERRUPT) == LOW) {
    receive_data.timer_us = micros();
    receive_data.state = 1;
  } else {
    // High after low - a bit has just finished.
    unsigned long diff_us = micros() - receive_data.timer_us;
    receive_data.state = 2;
    receive_data.message_ready = false;

    int value = -1;

    if (diff_us < MIN_ZERO_TIME) {
      // Pulse too short.
      receive_data.data_state = 1;
      return;
    } else if (diff_us <= MAX_ZERO_TIME) {
      receive_data.four_bits *= 2;
      ++receive_data.num_bits;
      value = 0;
    } else if (diff_us < MIN_ONE_TIME) {
      // Incorrect pulse - inbetween zero and one.
      receive_data.data_state = 1;
      return;
    } else if (diff_us <= MAX_ONE_TIME) {
      receive_data.four_bits *= 2;
      receive_data.four_bits += 1;
      ++receive_data.num_bits;
      value = 1;
    } else {
      // Pulse too long.
      receive_data.data_state = 0;
      return;
    }

    if (false) {
      Serial.print("Diff us: ");
      Serial.print(diff_us);
      Serial.print(" , num bits: ");
      Serial.print(receive_data.num_bits);
      if (value > -1) {
        Serial.print(", bit: ");
        Serial.println(value);
      } else {
        Serial.println();
      }
    }

    if ((receive_data.num_bits % 4) == 0) {
      char data_string[4];
      sprintf(data_string,"%X", receive_data.four_bits);
      receive_data.message[receive_data.num_chars] = data_string[0];
      
      receive_data.four_bits = 0;
      receive_data.num_bits = 0;
      ++receive_data.num_chars;
    }

    receive_data.state = 4;
    receive_data.timer_us = micros();
  }
}
