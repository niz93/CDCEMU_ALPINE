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

// Pick input and output PINs. On Atmega8, it's gonna be PB2 and PB4.
#define MBUS_IN_PIN 10 // Input port.
#define MBUS_OUT_PIN 12 // Output port.

#define UPDATE_CYCLE_MS 500

#define NUM_TRACKS 19
#define DISC_TOTAL_TIME 500

// Construct an MBus objects this example will work with.
MBus mbus(MBUS_IN_PIN, MBUS_OUT_PIN);

uint64_t received_message = 0;
uint8_t current_cd = 1;
uint8_t current_track = 1;
uint64_t current_track_time = 0;

uint64_t last_update_time_ms = 0;
boolean is_on = true;

MBus::PlayState play_state = MBus::PlayState::kPlaying;

//boolean    ignoreNext=false;
//boolean   wasCD=false;


//uint64_t  lastMessage=0;

//uint64_t  timeout=0;
//boolean   isOn=true;


void setup() {
  Serial.begin(9600);

  //mbus.sendInit();
  //mbus.sendAvailableDiscs();

  // Start playing the disc according to the defaults.
  mbus.sendChangingDisc(current_cd, current_track, 
                        MBus::ChangingStatus::kDone); 
  mbus.sendDiscInfo(current_cd, NUM_TRACKS, DISC_TOTAL_TIME);
  mbus.sendPlayingTrack(current_track, current_track_time, play_state);
}


/***************************************************************
loop
****************************************************************/
void loop() {
  uint64_t current_time_ms = millis();
	if (millis() > last_update_time_ms + UPDATE_CYCLE_MS && is_on) {
    if (play_state == MBus::PlayState::kPlaying) {
      current_track_time += current_time_ms - last_update_time_ms;
    }
    mbus.sendPlayingTrack(current_track, (uint16_t)(current_track_time / 1000), play_state);  
		last_update_time_ms = millis();
	}

	if (mbus.receive(&received_message)) {
    // printMessage(received_message);
     
//		if(ignoreNext)
//		{
//			ignoreNext=false;
//			if(wasCD)
//			{
//				mbus.sendDiscInfo(1, NUM_TRACKS, 500);
//				wasCD=false;
//			}
//		}
   
		if(received_message == Ping || received_message == Status) {
      // Acknowledge the ping message.
			mbus.send(PingOK);
		} else if (received_message == Shutdown && is_on) {
      Serial.write("off\n");
      // Acknowledge.
			mbus.send(Wait);
			is_on = false;
		} else if(received_message == Play) {
      Serial.write("play\n");
      is_on = true;
      play_state = MBus::PlayState::kPlaying;
      // Clear any error codes.
      mbus.sendChangerErrorCode(MBus::ChangerErrorCode::kNormal);   
		} else if(received_message == Pause) {
      Serial.write("pause\n");
      play_state = MBus::PlayState::kPaused;
    } else if(received_message == Stop) {
      Serial.write("stop\n");
      play_state = MBus::PlayState::kStopped;
    } else if(received_message == FastFwd || received_message == FastFwdPause) {
      Serial.write("ffwd\n");
			mbus.send(Wait);  
		} else if(received_message == FastBwd || received_message == FastBwdPause) {
      Serial.write("fbwd\n");
			mbus.send(Wait);
		} else if (received_message >> (4*5) == 0x113) {
      Serial.write("set\n");
      mbus.interpretSetDiskTrackMessage(received_message);
		}
//		else if(received_message>>(4*5)==0x113)//'please change cd'
//		{
//			uint64_t test=(received_message >>(4*4))-(uint64_t)0x1130; 
//			if(test>0)//not the same cd as before
//			{=
//				mbus.sendChangingDisc(test, 1, MBus::ChangingStatus::kDone);//'did change'
//				delay(50);
//				mbus.sendDiscInfo(1, NUM_TRACKS, 500);
//				currentCD=test;
//				currentTrack=1;
//				wasCD=true;
//				
//				if(timeout<millis())//debounce
//				{
//					//interpretDisc(currentCD);
//					timeout=millis()+1000;
//				}
//			}
//			else//same cd, maybe different track
//			{
//				uint8_t lastTrack=currentTrack;
//				//currentTrack=(received_message&((uint64_t)0xF<<(4*2)))>>(4*2);
//				currentTrack+=((received_message&((uint64_t)0xF<<(4*3)))>>(4*3))*10;
//				
//				mbus.sendChangingDisc(currentCD,currentTrack, MBus::ChangingStatus::kDone); //'still at cd...'
//				delay(50);
//				mbus.sendDiscInfo(1, NUM_TRACKS, 500);
//				delay(50);
//				mbus.sendPlayingTrack(currentTrack,0);
//				if(timeout<millis())//debounce
//				{
//					if(currentTrack!=lastTrack&&currentTrack>1)
//					{
//						//fwOrBw(lastTrack, currentTrack);
//					}
//					else if(currentTrack==1)
//					{
//						//interpretDisc(currentCD);
//						wasCD=true;
//					}
//					else
//					{
//						/* Error */
//					}
//					timeout=millis()+1000;
//				}
//			}
//			ignoreNext=true;
//		}
	}
}
