/***
MBus.cpp - Library to emulate Alpine M-Bus commands

Copyright 2012 Oliver Mueller

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

#ifndef MBUS_H
#define MBUS_H

#include "Arduino.h"

#define Shutdown 11142
#define Ping 0x18
#define	Play 0x11101
#define	Pause 0x11102
#define	Stop 0x11140
#define	FastFwd 0x11104
#define	FastBwd 0x11108
#define PingOK 0x98
#define Wait 0x9F00000
#define Off 0x11142
#define Changing 0x9B910100001

class MBus {
	public:
		MBus(uint8_t in, uint8_t out);
		void send(const uint64_t message);
		boolean receive(uint64_t* message);
		void sendPlayingTrack(uint8_t Track,uint16_t Time);
		void sendChangedCD(uint8_t CD,uint8_t Track);
		void sendCDStatus(uint8_t CD);

	private:
		void sendZero();
		void sendOne();
		void writeHexBitwise(uint8_t message);
		boolean checkParity(uint64_t* message);

		uint8_t pin_in_;
		uint8_t pin_out_;
};

#endif
