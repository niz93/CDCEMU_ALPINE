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

// Construct an MBus objects this example will work with.
MBus mBus(MBUS_IN_PIN, MBUS_OUT_PIN);

void setup() {
  Serial.begin(9600);
  Serial.write("hello world\n");
	//default to cd 1 track 1
	mBus.sendChangingDisc(1,1, MBus::ChangingStatus::kDone); 
	//mBus.sendCDStatus(1);
	mBus.sendPlayingTrack(1,0);
}
/***************************************************************
loop
****************************************************************/
void interpretDisc(int disc) //set your function per cd-button here
{
	switch(disc)
	{
	case 1:
		/*do something*/
		break;
	case 2:
		/*do something*/
		break;
	case 3:
		/*do something*/
		break;
	case 4:
		/*do something*/    
		break;
	case 5:
		/*do something*/
		break;
	case 6:
		/*do something*/
		break;
	default:
		break; 
	}
}


void fwOrBw(uint8_t oldTrack, uint8_t newTrack) // to interpret track changes as consecutive forward/backward skips
{
	uint8_t i=0;
	if(oldTrack<newTrack)
	{
		for(i=oldTrack;i<newTrack;i++)
		{
			/*skip forward*/
		}
	}
	else
	{
		for(i=newTrack;i<oldTrack;i++)
		{
			/*skip backwards*/
		}
	}
}

boolean		ignoreNext=false;
boolean		wasCD=false;
uint8_t		currentCD=1;
uint8_t		currentTrack=1;
uint64_t	receivedMessage=0;
uint64_t	lastMessage=0;
uint64_t	nextTime=0;
uint64_t	timeout=0;
boolean		isOn=true;

void loop()
{
	if(nextTime<millis()&&isOn)
	{
    Serial.write("hello world loop\n");
		mBus.sendPlayingTrack(currentTrack,(uint16_t)(millis()/1000)); 
		nextTime=millis()+500;
	}
	if(mBus.receive(&receivedMessage))
	{
    unsigned long long1 = (unsigned long)((receivedMessage & 0xFFFF0000) >> 16 );
    unsigned long long2 = (unsigned long)((receivedMessage & 0x0000FFFF));
    String hex = String(long1, HEX) + String(long2, HEX) + "\n"; // six octets
    Serial.print(hex);
     
		if(ignoreNext)
		{
			ignoreNext=false;
			if(wasCD)
			{
				mBus.sendDiscInfo(currentCD);
				wasCD=false;
			}
		}
		else if(receivedMessage == Ping)
		{
			mBus.send(PingOK);//acknowledge Ping
		}
		else if(receivedMessage == 0x19)
		{
			mBus.sendChangingDisc(currentCD,currentTrack, MBus::ChangingStatus::kDone); //'still at cd ...'
			delay(50);
			mBus.sendDiscInfo(currentCD);
			delay(50);
			mBus.sendPlayingTrack(currentTrack,0);
		}
		else if(receivedMessage==Off && isOn)
		{
			/*do something before shutdown*/
			mBus.send(Wait);//acknowledge
			isOn=false;
		}
		else if(receivedMessage==Play)
		{
      //  never executed?
			mBus.sendPlayingTrack(currentTrack,(uint16_t)(millis()/1000));    
      mBus.sendChangerErrorCode(MBus::ChangerErrorCode::kNormal);
		}
    else if(receivedMessage==Pause)
    {
      Serial.write("pause\n");
      //mBus.sendPlayingTrack(currentTrack,(uint16_t)(millis()/1000 - 10));    
      //mBus.sendChangerErrorCode(MBus::ChangerErrorCode::kHighTemperature);
      //mBus.sendChangingDisc(5, 23, MBus::ChangingStatus::kNoTrack);
    }
		else if(receivedMessage == FastFwd)
		{
      Serial.write("ffwd\n");
			mBus.send(Wait);//acknowledge
			/* do something on ffwd button*/  
      ++currentTrack;   
		}
		else if(receivedMessage == FastBwd)
		{
      Serial.write("fbwd\n");
			mBus.send(Wait);//acknowledge
			/* do something on fbwd button*/        
		}
		else if(receivedMessage>>(4*5)==0x113)//'please change cd'
		{
			uint64_t test=(receivedMessage >>(4*4))-(uint64_t)0x1130; 
			if(test>0)//not the same cd as before
			{
				mBus.sendChangingDisc(test, 1, MBus::ChangingStatus::kDone);//'did change'
				delay(50);
				mBus.sendDiscInfo(currentCD);
				currentCD=test;
				currentTrack=1;
				wasCD=true;
				
				if(timeout<millis())//debounce
				{
					interpretDisc(currentCD);
					timeout=millis()+1000;
				}
			}
			else//same cd, maybe different track
			{
				uint8_t lastTrack=currentTrack;
				//currentTrack=(receivedMessage&((uint64_t)0xF<<(4*2)))>>(4*2);
				currentTrack+=((receivedMessage&((uint64_t)0xF<<(4*3)))>>(4*3))*10;
				
				mBus.sendChangingDisc(currentCD,currentTrack, MBus::ChangingStatus::kDone); //'still at cd...'
				delay(50);
				mBus.sendDiscInfo(currentCD);
				delay(50);
				mBus.sendPlayingTrack(currentTrack,0);
				if(timeout<millis())//debounce
				{
					if(currentTrack!=lastTrack&&currentTrack>1)
					{
						fwOrBw(lastTrack, currentTrack);
					}
					else if(currentTrack==1)
					{
						interpretDisc(currentCD);
						wasCD=true;
					}
					else
					{
						/* Error */
					}
					timeout=millis()+1000;
				}
			}
			ignoreNext=true;
		}
	}
}
