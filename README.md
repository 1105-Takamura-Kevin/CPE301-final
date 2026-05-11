# CPE301.1001 Final Project
Group 18: Kevin Takamura
<br><br>
AceCard is an RFID-based transit fare system, built using the Arduino Mega 2560.
The demonstration video can be found [here](). Add the real link!!!
## Event codes
For easy referencing, the `printEvent(__)` event codes are listed here.
| ID | Event |
|----|-------|
| 0 | The system started successfully. |
| 1 | The system could not start: RTC could not connect. Try checking the wiring. |
| 2 | The system could not start: RFID could not connect. Try checking the wiring. |
| 3 | The state changed to OFF. |
| 4 | The state changed to IDLE. |
| 5 | The state changed to ACTIVE. |
| 6 | The state changed to ERROR. |
| 7 | An improperly formatted card was scanned. |
| 8 | A correctly formatted card was scanned. |
| 9 | The reset button was presssed. |
| 10 | The presented RFID card could not read/write. Try holding the card directly on the sensor for 3 seconds. |
| 11 | The presented RFID card was charged 1 credit. |
| 12 | The presented RFID card was charged 2 credits. |
| 13 | The presented RFID card was charged 3 credits. |
| 14 | The presented RFID card did not have enough credits for the set fare. |
