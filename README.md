## Alpine M-Bus library for Arduino

This is a modified and updated library forked from:
https://github.com/Olstyle/MBus

The changes include:
* added several new protocol messages, including more detailed changing messages and error messages
* some minor optimizations
* reformatted the code to fit the Google c++ styleguide I'm more used to
* (soon) updated the example file to fit my

Overall, the library fits on an Atmega8 with a MiniCore package (https://github.com/MCUdude/MiniCore) and was tested with an Alpine 7618R headunit.

### Original readme of Oliver Mueller

This is an Arduino library to emulate Alpines M-Bus which is used on a variety of car radios.
You can send and receive messages as well as send special messages which trick the radio in believing it communicates with an cd changer.

Electrically, you'd want to have a piece of circuitry to split the send and receive line which is shown here(not my website):
http://www.hohensohn.info/mbus/index.htm
I also used this site for reference on the bus itself.


### License

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
