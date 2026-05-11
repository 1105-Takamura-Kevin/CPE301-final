# CPE301.1001 Final Project
**Group 18:** Kevin Takamura
<br><br>
AceCard is an RFID-based transit fare system, built using the Arduino Mega 2560.  
The demonstration video can be found [here](). Add the real link!!!
<br><br>
**Please note:** the add30credits.ino file is **ONLY** a supplementary program used to load 15 credits onto an RFID card and should **NOT** be included in grading. It is a slightly modified version of an example program included with the MFRC522 library.
<br><br>
Additionally, CPE301final.ino is the main file and designed to only work with **ONE** RFID card ID. This is to simulate "authentic", genuine AceCards and allows me to demonstrate an ERROR state condition. To allow your RFID tag to be used with my program, the ID must be: `0xE0, 0x50, 0xA8, 0x5F`
## Event codes
For easy referencing, the `printEvent(__)` serial event codes are listed here.
| ID | Event |
|----|-------|
| 0 | The system started successfully. |
| 1 | The system could not start: RTC could not connect. Try checking the wiring. |
| 2 | The system could not start: RFID could not connect. Try checking the wiring. |
| 3 | The state changed to OFF. |
| 4 | The state changed to IDLE. |
| 5 | The state changed to ACTIVE. |
| 6 | The state changed to ERROR. |
| 7 | A counterfeit or improperly formatted card was scanned. |
| 8 | A genuine card was scanned. |
| 9 | The reset button was presssed. |
| 10 | The presented RFID card could not read/write. Try holding the card directly on the sensor for 3 seconds. |
| 11 | The presented RFID card was charged 1 credit. Allow rider to board. |
| 12 | The presented RFID card was charged 2 credits. Allow rider to board. |
| 13 | The presented RFID card was charged 3 credits. Allow rider to board. |
| 14 | The presented RFID card did not have enough credits for the set fare. Do not allow rider to board. |
