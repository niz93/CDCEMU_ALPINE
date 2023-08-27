/***
MBus.h - Library to emulate Alpine M-Bus commands

Copyright 2012 Oliver Mueller
Copyright 2020 Marcin Dymczyk

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
***/

#include "mbus.h"

void printMessage(uint64_t msg) {
  // unsigned long long1 = (unsigned long)((msg & 0xFFFF0000) >> 16);
  // unsigned long long2 = (unsigned long)((msg & 0x0000FFFF));
  // String hex = String(long1, HEX) + String(long2, HEX) + "\n"; // six octets
  // Serial.print(hex);
  char message_char[17];
  sprintf(message_char, "%8lx%08lx", ((uint32_t)((msg >> 32) & 0xFFFFFFFF)), ((uint32_t)(msg & 0xFFFFFFFF)));
  Serial.print(message_char);
}

MBus::MBus(uint8_t pin_in, uint8_t pin_out) : pin_in_(pin_in),
                                              pin_out_(pin_out) {
  pin_in_ = pin_in;
  pin_out_ = pin_out;

  pinMode(pin_in_, INPUT);
  pinMode(pin_out_,OUTPUT);
}

void MBus::sendZero() {
  digitalWrite(pin_out_, HIGH);
  delayMicroseconds(600);
  digitalWrite(pin_out_, LOW);
  delayMicroseconds(2400);
}

void MBus::sendOne() {
  digitalWrite(pin_out_, HIGH);
  delayMicroseconds(1800);
  digitalWrite(pin_out_, LOW);
  delayMicroseconds(1200);
}

void MBus::writeHexBitwise(uint8_t message) {
  for (int8_t i = 3; i > -1; i--) {
    uint8_t output = ((message & (1 << i)) >> i);

    if (output == 1) {
      sendOne();
    } else {
      sendZero();
    }
  }
}

boolean MBus::checkParity(uint64_t* message) {
  uint8_t parity = 0;
  uint8_t test = 0;
  for(uint8_t i = 16; i > 0; i--) {
    test = ((*message & ((uint64_t)0xF << i*4)) >> i*4);
    parity = parity^test;
  }
  ++parity;

  return parity == (*message & ((uint64_t)0xF));
}

void MBus::send(const uint64_t message) {
  Serial.print("sending: ");
  printMessage(message);
  Serial.println();

  uint8_t printed = 0;
  uint8_t parity = 0;
  for(int8_t i = 16; i >= 0; i--) {
    uint8_t output = ((message & ((uint64_t)0xF << i * 4)) >> i * 4);
    parity = parity ^ output;
    if (!output && !printed) {
      // Do nothing.
    } else {
      writeHexBitwise(output);
      ++printed;
    }
  }
  ++parity;
  writeHexBitwise(parity);
}

boolean MBus::receive(uint64_t* message) {
  *message = 0;
  if (digitalRead(pin_in_) == LOW) {
    unsigned long bit_start_time_us = micros();

    boolean is_bit_in_progress = false;
    uint8_t counter = 0;

    while ((micros() - bit_start_time_us) < 4000) {
      if (digitalRead(pin_in_) == HIGH && !is_bit_in_progress) {
        const uint16_t bit_delta_time_us = micros() - bit_start_time_us;
        if (bit_delta_time_us < 1400 && bit_delta_time_us > 600) {
          // 0 is in between 600 and 1700 microseconds.
          *message *= 2;
          ++counter;
          is_bit_in_progress = true;
        } else if (bit_delta_time_us > 1400) {
          // 1 is longer than 1700 microseconds.
          *message *= 2;
          *message += 1;
          ++counter;
          is_bit_in_progress = true;
        }
      }
      if (is_bit_in_progress && digitalRead(pin_in_) == LOW) {
        is_bit_in_progress = false;
        bit_start_time_us = micros();
      }
    }

    if (counter % 4 || !checkParity(message) || counter == 0) {
      // Message is not ok.
      *message = 0;
      return false;
    } else {
      // Ignore parity.
      (*message) = (*message) >> 4;
      return true;
    }
  }

  // The input pin is not low so no message could be read.
  return false;
}

// -----------------------------------
// CD-changer emulation from here on.
// -----------------------------------

void MBus::sendPlayingTrack(uint8_t track_number, uint16_t track_time_sec, PlayState play_state) {
  uint64_t play = 0x990000100000001ull;

  play |= (uint64_t)play_state << (12 * 4);

  play |= (uint64_t)(track_number % 10) << (10 * 4);
  play |= (uint64_t)(track_number / 10) << (11 * 4);

  play |= (uint64_t)(track_time_sec % 10) << (4 * 4);
  play |= (uint64_t)((track_time_sec % 100) / 10) << (5 * 4);
  play |= (uint64_t)((track_time_sec / 60) % 10) << (6 * 4);
  play |= (uint64_t)(((track_time_sec / 60) % 100) / 10) << (7 * 4);


  // TODO wrap in a function.
  char state_str[12];
  switch (play_state) {
    case kPreparing:
      strcpy(state_str, "preparing");
      break;
    case kStopped:
      strcpy(state_str, "stopped");
      break;
    case kPaused:
      strcpy(state_str, "paused");
      break;
    case kPlaying:
      strcpy(state_str, "playing");
      break;
    case kSpinup:
      strcpy(state_str, "spinup");
      break;
    case kFastForward:
      strcpy(state_str, "ffwd");
      break;
    case kFastReverse:
      strcpy(state_str, "frev");
      break;
    default:
      strcpy(state_str, "unknown");
  }

  char message_char[35];
  sprintf(message_char, "CDC: t%d, %d track time sec, %s", track_number, track_time_sec, state_str);
  Serial.println(message_char);

  // if (play_state == PlayState::kPaused) {
  //   send(0x993060100030002ull);
  // }

  send(play);
}

void MBus::sendChangingDisc(uint8_t disc_number, uint8_t track_number,
                            ChangingStatus changing_status) {
  uint64_t changed_disc_message = 0x9B000000001ull;
  // The 0x9Bg header means a disc changing operation. The value g=9 it means
  // the changing is done.

  // TODO wrap in a function.
  // char change_status_str[12];
  // switch (changing_status) {
  //   case kInProgress:
  //     strcpy(change_status_str, "in progress");
  //     break;
  //   case kNoMagazine:
  //     strcpy(change_status_str, "no mag");
  //     break;
  //   case kNoDisc:
  //     strcpy(change_status_str, "no disc");
  //     break;
  //   case kNoTrack:
  //     strcpy(change_status_str, "no track");
  //     break;
  //   case kDone:
  //     strcpy(change_status_str, "done");
  //     break;
  //   default:
  //     strcpy(change_status_str, "unknown");
  // }

  // char message_char[35];
  // sprintf(message_char, "Disc change: t%d, d%d, %s", track_number, disc_number, change_status_str);
  // Serial.println(message_char);

  changed_disc_message |= (uint64_t)disc_number << (7 * 4);

  changed_disc_message |= (uint64_t)(track_number % 10) << (5 * 4);
  changed_disc_message |= (uint64_t)(track_number / 10) << (6 * 4);

  switch (changing_status) {
    case kInProgress: {
      changed_disc_message |= (uint64_t)4 << (8 * 4);
    }
    case kNoMagazine: {
      changed_disc_message |= (uint64_t)4 << (8 * 4);
      changed_disc_message |= (uint64_t)2 << (4 * 4);
    }
    case kNoDisc: {
      changed_disc_message |= (uint64_t)4 << (8 * 4);
      changed_disc_message |= (uint64_t)4 << (4 * 4);
      break;
    }
    case kNoTrack: {
      changed_disc_message |= (uint64_t)4 << (8 * 4);
      changed_disc_message |= (uint64_t)1 << (4 * 4);
      break;
    }
    case kDone:
    default: {
      changed_disc_message |= (uint64_t)9 << (8 * 4);
      break;
    }
  }
  send(changed_disc_message);
}

void MBus::sendDiscInfo(uint8_t disc_number, uint8_t total_tracks,
                        uint16_t total_time_sec) {
  uint64_t disc_info_message = 0x9C001000000Full;

  disc_info_message |= ((uint64_t)disc_number << (9 * 4));

  disc_info_message |= ((uint64_t)(total_tracks / 10) << (6 * 4));
  disc_info_message |= ((uint64_t)(total_tracks % 10) << (5 * 4));

  disc_info_message |= (uint64_t)(total_time_sec % 10) << (4 * 4);
  disc_info_message |= (uint64_t)((total_time_sec % 100) / 10) << (3 * 4);
  disc_info_message |= (uint64_t)((total_time_sec / 60) % 10) << (2 * 4);
  disc_info_message |=
    (uint64_t)(((total_time_sec / 60) % 100) / 10) << (1 * 4);

  send(disc_info_message);
}

void MBus::sendChangerErrorCode(ChangerErrorCode code) {
  uint64_t error_message = 0x9F00000ull;

  switch (code) {
    case kNormal: {
      break;
    }
    case kHighTemperature: {
      error_message |= (uint64_t)3 << (4 * 4);
      break;
    }
    case kDiscChangeIssue: {
      error_message |= (uint64_t)1 << (3 * 4);
      break;
    }
    case kStuckDisc: {
      error_message |= (uint64_t)2 << (3 * 4);
    }
    default: {
      break;
    }
  }
  send(error_message);
}

MBus::DiskTrackChange MBus::interpretSetDiskTrackMessage(const uint64_t message) {
  // 7618: 113dttff
  // 7909: 113dttf
  //Serial.println((uint64_t)(message >> (4*5)));

  DiskTrackChange out_data;

  if ((message >> (4*5)) == 0x113) {
    Serial.println("setDiskTrack format 7618.");
    out_data.disc = (message & ((uint64_t)0xF << (4*4))) >> (4*4);
    out_data.track = (message & ((uint64_t)0xF << (4*2))) >> (4*2);
    out_data.track +=((message&((uint64_t)0xF<<(4*3)))>>(4*3))*10;
  } else if ((message >> (4*4)) == 0x113) {
    Serial.println("setDiskTrack format 7909.");
    out_data.disc = (message & ((uint64_t)0xF << (4*3))) >> (4*3);
    out_data.track = (message & ((uint64_t)0xF << (4*1))) >> (4*1);
    out_data.track +=((message&((uint64_t)0xF<<(4*2)))>>(4*2))*10;
  } else {
    Serial.println("Incorrect setDiskTrack format.");
  }

  char received_message_char[18];
  sprintf(received_message_char, "%08lX", (message >> (4*5)));  
  Serial.println(received_message_char);

  char data_char[18];
  sprintf(data_char, "Change: d=%d t=%d", out_data.disc, out_data.track);  
  Serial.println(data_char);

  return out_data;
}

void MBus::sendInit() {
  uint64_t init_message = 0x9A0000000000;
  send(init_message);

  uint64_t init_message3 = 0x9D00000000;
  send(init_message3);

  uint64_t init_message4 = 0x9E0000000;
  send(init_message4);
}

void MBus::sendWait() {
  uint64_t wait_message = Wait;
  send(wait_message);  
}

void MBus::sendAvailableDiscs() {
  uint64_t available_disc_message =   0x9D00000000;
  //uint64_t available_disc_message = 0x9D000D69216;
  //uint64_t available_disc_message = 0x9D000D38113;
  send(available_disc_message);
}
