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

// Construct an MBus objects this example will work with.
MBus mbus(MBUS_IN_PIN, MBUS_OUT_PIN);

uint8_t current_disc = 1;
uint8_t current_track = 1;
uint64_t current_track_time = 0;

uint64_t last_update_time_ms = 0;
boolean is_on = true;

uint8_t num_stop_pause_messages = 0;

volatile byte led_state = LOW;

MBus::PlayState play_state = MBus::PlayState::kStopped;

struct MbusReceiveData {
  byte state;
  byte data_state;
  uint8_t num_bits;
  unsigned long timer_us;

  uint8_t num_chars;
  volatile bool message_ready;
  volatile uint64_t message;
};
volatile MbusReceiveData receive_data = {0, 0, 0, 0, 0, 0, false};

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
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), MBus::PlayState::kPreparing);  
      play_state = MBus::PlayState::kPaused;
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state); 
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state);  
    }
  } else if (received_message == Stop) {
    Serial.println(F("HU: Stop"));
    if (play_state != MBus::PlayState::kStopped) {
      mbus.sendWait();
      play_state = MBus::PlayState::kStopped;
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state);
      delay(200);
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
    //mbus.sendChangerErrorCode(MBus::ChangerErrorCode::kNormal);
  } else if (received_message >> (4*5) == ChangePrefix || received_message >> (4*4) == ChangePrefix) {
    Serial.println(F("Change"));
    mbus.sendWait();
    MBus::DiskTrackChange change = mbus.interpretSetDiskTrackMessage(received_message);
    if (change.disc == 0) change.disc = current_disc;
    if (change.track == 0) change.track = current_track;

    current_disc = change.disc;
    current_track = change.track;

    current_track_time = current_disc * 10000;
    
    mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), MBus::PlayState::kPreparing); 
    mbus.sendWait();
    
    mbus.sendChangingDisc(current_disc, current_track, MBus::ChangingStatus::kInProgress);
    mbus.sendWait();
    delay(50);
    mbus.sendChangingDisc(current_disc, current_track, MBus::ChangingStatus::kDone);
    mbus.sendWait();
    mbus.sendDiscInfo(current_disc, NUM_TRACKS, DISC_TOTAL_TIME);
  } else {
    Serial.print(F("Other message: "));

    char received_message_char[18];
    sprintf(received_message_char, "%08llX", received_message);  
    Serial.print(received_message_char);
    Serial.println();
  }

  receive_data.message_ready = false;
  receive_data.message = 0;
}

void checkFinished() {
  // We should move this to an interrupt routine one day.
  if (!receive_data.message_ready && receive_data.state == 4 && (micros() - receive_data.timer_us) > 4000) {
    const bool parity_ok = mbus.checkParity((uint64_t*)&receive_data.message);

    if (parity_ok) {
       // Cut the 4 bits parity and flag the message as ready.
      receive_data.message = receive_data.message >> 4;
      receive_data.message_ready = true;
    } else {
      // CRC failed, we will not flag the message as ready and reset the struct.
      Serial.print(F("CRC Error: "));
      printMessage(receive_data.message);
      Serial.println();
    }

    // Reset the state as the timeout happened. 
    receive_data.state = 0;
    receive_data.num_bits = 0;
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

    if (diff_us < MIN_ZERO_TIME) {
      // Pulse too short.
      receive_data.data_state = 1;
      return;
    } else if (diff_us <= MAX_ZERO_TIME) {
      receive_data.message *= 2;
    } else if (diff_us < MIN_ONE_TIME) {
      // Incorrect pulse - inbetween zero and one.
      receive_data.data_state = 2;
      return;
    } else if (diff_us <= MAX_ONE_TIME) {
      receive_data.message *= 2;
      receive_data.message += 1;
    } else {
      receive_data.data_state = 3;
      // Pulse too long.
      return;
    }

    ++receive_data.num_bits;

    if ((receive_data.num_bits % 4) == 0) {
      receive_data.num_bits = 0;
      ++receive_data.num_chars;
    }

    receive_data.state = 4;
    receive_data.timer_us = micros();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Init");

  mbus.sendInit();
  mbus.sendAvailableDiscs();

  // Start playing the disc according to the defaults.
  mbus.sendChangingDisc(current_disc, current_track, 
                        MBus::ChangingStatus::kDone); 
  mbus.sendDiscInfo(current_disc, NUM_TRACKS, DISC_TOTAL_TIME);
  mbus.sendPlayingTrack(current_track, current_track_time, play_state);

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
    handleMbusMessage(receive_data.message);
  }

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
      mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state);
      
      if (play_state == MBus::PlayState::kStopped || play_state == MBus::PlayState::kPaused) {
        // Increment the counter if we're actually in the stop/pause mode.
        ++num_stop_pause_messages;
      }
    }

    last_update_time_ms = millis();
  }
  interrupts();
}
