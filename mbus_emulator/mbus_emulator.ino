// Copyright 2012 Oliver Mueller
// Copyright 2020 Marcin Dymczyk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.



// TODOs
// 1) [ ] Repeat/repeat all/mix/scan
// 2) [x] Disc change doesn't work (on 7618R)
// 3) [x] Still sometimes screw-ups with CRC error
// 4) [x] Test with 7909
// 5) [x] Why is there so much delay when changing track/disc?
// 6) [x] Why stray bits registered all the time?


#include <mbus.h>
#include <TimerOne.h>

// Adapted to Teensy.
#define MBUS_IN_PIN 2 // Input port.
#define MBUS_IN_PIN_INTERRUPT MBUS_IN_PIN
#define MBUS_OUT_PIN 3 // Output port.
#define LED_OUT_PIN LED_BUILTIN // Output port.

#define UPDATE_CYCLE_MS 500

#define NUM_TRACKS 19
#define DISC_TOTAL_TIME 500

#define DEFAULT_ZERO_TIME 600
#define DEFAULT_ONE_TIME  1900
#define DEFAULT_TOLERANCE 450

#define MIN_ZERO_TIME (DEFAULT_ZERO_TIME - DEFAULT_TOLERANCE)
#define MAX_ZERO_TIME (DEFAULT_ZERO_TIME + DEFAULT_TOLERANCE)
#define MIN_ONE_TIME (DEFAULT_ONE_TIME - DEFAULT_TOLERANCE)
#define MAX_ONE_TIME (DEFAULT_ONE_TIME + DEFAULT_TOLERANCE)

enum MbusDataState {
  kNoMessage,
  kBitStart,
  kCorrectBitEnd,
  kEarlyBitEnd,
  kLateBitEnd,
  kCharEnd,
  kUnknown
};

// Construct an MBus objects this example will work with.
MBus mbus(MBUS_IN_PIN, MBUS_OUT_PIN);

uint8_t current_disc = 1;
uint8_t current_track = 1;
uint64_t current_track_time = 0;

uint64_t last_update_time_ms = 0;
boolean is_on = true;

uint8_t num_stop_pause_messages = 0;

MBus::PlayState play_state = MBus::PlayState::kStopped;

struct MbusReceiveData {
  MbusDataState state = MbusDataState::kNoMessage;
  volatile bool message_ready;

  uint8_t num_bits_of_current_char;
  uint8_t num_chars;
    
  unsigned long timer_us;
  unsigned long last_interrupt_timer_us;

  volatile uint64_t message;
};
volatile MbusReceiveData receive_data = {MbusDataState::kNoMessage, false, 0, 0, 0, 0, 0};

void handleMbusMessage(volatile uint64_t received_message) {
  if(received_message == Ping || received_message == Status) {
    // Acknowledge the ping message.
    mbus.send(PingOK);
    Serial.println(F("HU: Ping"));
  } else if (received_message == Resume) {
    mbus.sendDiscInfo(current_disc, NUM_TRACKS, 500);
    Serial.println(F("HU: Resume"));
    play_state = MBus::PlayState::kPlaying;
  } else if (received_message == ResumeP) {
    mbus.sendDiscInfo(current_disc, NUM_TRACKS, 500);
    Serial.println(F("HU: Resume pause"));
  } else if(received_message == Pause) {
    Serial.println(F("HU: Pause"));
    if (play_state != MBus::PlayState::kPaused) {
      mbus.sendWait();
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), MBus::PlayState::kPreparing);
      delayMicroseconds(3000);
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), MBus::PlayState::kPreparing);  
      play_state = MBus::PlayState::kPaused;
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state); 
      delayMicroseconds(3000);
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state);  
    }
  } else if (received_message == Stop) {
    Serial.println(F("HU: Stop"));
    if (play_state != MBus::PlayState::kStopped) {
      mbus.sendWait();
      play_state = MBus::PlayState::kStopped;
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state);
      delayMicroseconds(3000);
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state);
    }
  } else if (received_message == Shutdown && is_on) {
    // Acknowledge.
    mbus.send(Wait);
    Serial.println(F("HU: Shutdown"));
    is_on = false;
  } else if(received_message == Play) {
    Serial.println(F("HU: Play"));
    is_on = true;
    play_state = MBus::PlayState::kPlaying;
    // Clear any error codes.
    // mbus.sendChangerErrorCode(MBus::ChangerErrorCode::kNormal); // Not necessary?
  } else if (received_message >> (4*5) == ChangePrefix || received_message >> (4*4) == ChangePrefix) {
    Serial.println(F("Change"));
    mbus.sendWait();
    delayMicroseconds(3000);

    bool change_disc = false;

    MBus::DiskTrackChange change = mbus.interpretSetDiskTrackMessage(received_message);
    if (change.disc == 0) {
      change.disc = current_disc;
    } else {
      change_disc = true;
    }
    if (change.track == 0) {
      change.track = current_track;
    }

    current_disc = change.disc;
    current_track = change.track;
    current_track_time = current_disc * 10000;

    // Experimental delays below to make 7618 accept the disc changes properly.
    if (change_disc) {
      mbus.sendChangingDisc(current_disc, current_track, MBus::ChangingStatus::kDone);
      delayMicroseconds(3000);
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), MBus::PlayState::kPreparing); 

      mbus.sendChangingDisc(current_disc, current_track, MBus::ChangingStatus::kInProgress);
      delayMicroseconds(3000);
      mbus.sendWait();
    }
    
    mbus.sendChangingDisc(current_disc, current_track, MBus::ChangingStatus::kDone);
    delayMicroseconds(3000);

    if (change_disc) {
      mbus.sendDiscInfo(current_disc, NUM_TRACKS, DISC_TOTAL_TIME);
      delayMicroseconds(3000);
      mbus.sendWait();
    }
    
    mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), MBus::PlayState::kSpinup); 
    delayMicroseconds(3000);
    mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), MBus::PlayState::kPlaying); 
  } else {
    Serial.print(F("Other message: "));

    char received_message_char[18];
    sprintf(received_message_char, "%08llX", received_message);  
    Serial.print(received_message_char);
    Serial.println();
  }

  // Fully reset the state.
  receive_data.message_ready = false;
  receive_data.message = 0;

  Serial.send_now();
}

void checkFinished() {
  // We should move this to an interrupt routine one day.
  if (!receive_data.message_ready && (micros() - receive_data.last_interrupt_timer_us) > 3000) {
    if (receive_data.state == MbusDataState::kCharEnd) {
      const bool parity_ok = mbus.checkParity((uint64_t*)&receive_data.message);
  
      if (parity_ok) {
        // Cut the 4 bits parity and flag the message as ready.
        receive_data.message = receive_data.message >> 4;
        receive_data.message_ready = true;
        Serial.println("Parity OK, marking message as ready.");
      } else {
        // CRC failed, we will not flag the message as ready and reset the struct.
        Serial.print(F("CRC Error: "));
        printMessage(receive_data.message);
        receive_data.message = 0;
        Serial.println();
      }
    } else {
      // Reset the state as the timeout happened since the last interrupt trigger
      // and the message was not finished.
      receive_data.message = 0;
    }

    // Reset the state.
    receive_data.state = MbusDataState::kNoMessage;
    receive_data.num_bits_of_current_char = 0;
    receive_data.num_chars = 0;
  }
}

void handleMbusInterrupt() {
  receive_data.last_interrupt_timer_us = micros();

  if (receive_data.state == MbusDataState::kNoMessage && digitalRead(MBUS_IN_PIN_INTERRUPT) == HIGH) {
    // A stray bit we haven't registered has just finished, nothing to do here.
    // It's totally fine as it is very likely related to an interrupt triggered when we're sending data.
    return;
  }

  if (digitalRead(MBUS_IN_PIN_INTERRUPT) == LOW) {
    receive_data.timer_us = micros();
    receive_data.state = MbusDataState::kBitStart;
  } else {
    // High after low - a bit has just finished.
    const unsigned long diff_us = micros() - receive_data.timer_us;
    receive_data.state = MbusDataState::kCorrectBitEnd;
    receive_data.message_ready = false;

    if (diff_us < MIN_ZERO_TIME) {
      // Pulse too short.
      Serial.println(F("Bit too short"));
      receive_data.state = MbusDataState::kEarlyBitEnd;
      return;
    } else if (diff_us <= MAX_ZERO_TIME) {
      receive_data.message *= 2;
    } else if (diff_us < MIN_ONE_TIME) {
      // Incorrect pulse - inbetween zero and one.
      // We will ignore this and wait for the correct bit end.
      Serial.println(F("Bit in-between."));
      receive_data.state = MbusDataState::kEarlyBitEnd;
      return;
    } else if (diff_us <= MAX_ONE_TIME) {
      receive_data.message *= 2;
      receive_data.message += 1;
    } else {
      receive_data.state = MbusDataState::kLateBitEnd;
      Serial.println(F("Late bit end: ") + (String)diff_us + " [us]");
      // Pulse too long. Let's ignore this, assume it's a zero and continue.
      // The message will get rejected via parity anyways.
      receive_data.message *= 2;
    }

    ++receive_data.num_bits_of_current_char;

    if ((receive_data.num_bits_of_current_char % 4) == 0) {
      receive_data.num_bits_of_current_char = 0;
      ++receive_data.num_chars;
      receive_data.state = MbusDataState::kCharEnd;
    }
  }
}

void mbusSetup() {
  mbus.sendInit();
  mbus.sendAvailableDiscs();

  // Start playing the disc according to the defaults.
  mbus.sendChangingDisc(current_disc, current_track, 
                        MBus::ChangingStatus::kDone); 
  mbus.sendDiscInfo(current_disc, NUM_TRACKS, DISC_TOTAL_TIME);
  mbus.sendPlayingTrack(current_track, current_track_time, play_state);

  // The pullup on the interrupt pin is fundamental to avoid ringing.
  pinMode(MBUS_IN_PIN_INTERRUPT, INPUT_PULLUP);
  pinMode(LED_OUT_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(MBUS_IN_PIN_INTERRUPT), handleMbusInterrupt, CHANGE);

  receive_data.timer_us = micros();
  Timer1.initialize(1000);
  Timer1.attachInterrupt(checkFinished);
}

void mbusLoop() {
  noInterrupts();
  if (receive_data.message_ready) {
    Serial.println("About to handle the message.");
    digitalWrite(LED_OUT_PIN, HIGH); 
    handleMbusMessage(receive_data.message);
    digitalWrite(LED_OUT_PIN, LOW);
  }
  interrupts();

  uint64_t current_time_ms = millis();
  if (millis() > last_update_time_ms + UPDATE_CYCLE_MS && is_on) {
    if (play_state == MBus::PlayState::kPlaying) {
      current_track_time += current_time_ms - last_update_time_ms;
    }

    if (!(play_state == MBus::PlayState::kStopped || play_state == MBus::PlayState::kPaused)) {
      // If we're not in the stop or pause state, reset the counter.
      num_stop_pause_messages = 0;
    }

    if (num_stop_pause_messages < 2) {
      // Only send the stop/pause message twice, then the headunit will just ping us.
      while (!receive_data.state == MbusDataState::kNoMessage) {
        // Wait until no message is being received.
        ;
      }
      noInterrupts();
      digitalWrite(LED_OUT_PIN, HIGH); 
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state);
      digitalWrite(LED_OUT_PIN, LOW); 
      interrupts();
      
      if (play_state == MBus::PlayState::kStopped || play_state == MBus::PlayState::kPaused) {
        // Increment the counter if we're actually in the stop/pause mode.
        ++num_stop_pause_messages;
      }
    }
    last_update_time_ms = millis();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Init");

  mbusSetup();
}

void loop() {
  mbusLoop();
}
