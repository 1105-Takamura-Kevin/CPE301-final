// AceCard by Kevin Takamura - an RFID transit fare system
// Created for CPE 301.1001, submitted May 11, 2026

// NOTE: This project uses several libraries,
// but does not use any of the prohibited library functions. 

#include <MFRC522.h> // https://github.com/miguelbalboa/rfid
#include <LiquidCrystal.h> // https://github.com/arduino-libraries/LiquidCrystal
#include <SPI.h> // Installed with Arduino
#include <Wire.h> // Installed with Arduino
#include <RTClib.h> // https://github.com/adafruit/RTClib
#include <Buzzer.h> // https://github.com/gmarty2000-ARDUINO/arduino-BUZZER

// Timers
volatile unsigned char *myTCCR1A = (volatile unsigned char *)0x80;
volatile unsigned char *myTCCR1B = (volatile unsigned char *)0x81;
volatile unsigned char *myTCCR1C = (volatile unsigned char *)0x82;
volatile unsigned char *myTIMSK1 = (volatile unsigned char *)0x6F;
volatile unsigned char *myTIFR1  = (volatile unsigned char *)0x36;
volatile unsigned int  *myTCNT1  = (volatile unsigned int *)0x84;
volatile unsigned int  *myOCR1A  = (volatile unsigned int *)0x88;
// UART
volatile unsigned char *myUCSR0A = (volatile unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (volatile unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (volatile unsigned char *)0x00C2;
volatile unsigned char *myUBRR0  = (volatile unsigned char *)0x00C4;
volatile unsigned char *myUDR0   = (volatile unsigned char *)0x00C6;
// GPIO - Port B for pins 4-7, 50-53
volatile unsigned char *portB = (volatile unsigned char *)0x25;
volatile unsigned char *ddrB  = (volatile unsigned char *)0x24;
volatile unsigned char *pinB  = (volatile unsigned char *)0x23;
//      - Port F for pins A0-A7
volatile unsigned char *portF = (volatile unsigned char *)0x31;
volatile unsigned char *ddrF  = (volatile unsigned char *)0x30;
volatile unsigned char *pinF  = (volatile unsigned char *)0x2F;
//      - Port K for pins A8-A15
volatile unsigned char *portK = (volatile unsigned char *)0x108;
volatile unsigned char *ddrK  = (volatile unsigned char *)0x107;
volatile unsigned char *pinK  = (volatile unsigned char *)0x106;
//      - Port L for pins 42-49
volatile unsigned char *portL = (volatile unsigned char *)0x10B;
volatile unsigned char *ddrL  = (volatile unsigned char *)0x10A;
volatile unsigned char *pinL  = (volatile unsigned char *)0x109;
// ADC
volatile unsigned char *my_ADMUX    = (unsigned char*) 0x7C;
volatile unsigned char *my_ADCSRB   = (unsigned char*) 0x7B;
volatile unsigned char *my_ADCSRA   = (unsigned char*) 0x7A;
volatile unsigned int  *my_ADC_DATA = (unsigned int*)  0x78;
// Interrupt
volatile unsigned char *myPCICR  = (volatile unsigned char *)0x68;
volatile unsigned char *myPCMSK2 = (volatile unsigned char *)0x6D;
volatile unsigned char *myPCIFR  = (volatile unsigned char *)0x3B;

// Variables
const int RS = 49, EN = 48, D4 = 47, D5 = 46, D6 = 45, D7 = 44; // For LCD
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7); // Create LCD object
const int rfidRST = 11, rfidSS = 53; // For RFID
MFRC522 mfrc522(rfidSS, rfidRST); // Create RFID object
RTC_DS3231 rtc; // Create RTC object
volatile int systemState = 0, lastState = 5; // 0 = off, 1 = idle, 2 = active, 3 = error, 5 = arbitrary initial value
volatile unsigned int potValue; // Potentiometer value
unsigned int fareValue; // Price to ride the bus
bool screenUpdated = 0, success;
unsigned long previousMillis = 0, currentMillis; // Used for display refresh
const unsigned long interval = 60000; // 60,000ms = 1 minute for display refresh


void setup() {
  U0init(9600); // Start serial
  adc_init(); // Start ADC

  *ddrB  = 0b01100111; // PB0-2 for RFID OUT, PB3 for RFID IN, PB5 for RFID OUT, PB6 for buzzer OUT
  *portB = 0b00000000; // NO pull-ups
  *ddrF  = 0b00000000; // PF0 for potentiometer
  *portF = 0b00000000; // NO pull-ups
  *ddrK  = 0b00011000; // PK0-2 IN for buttons, PK3-4 OUT for yellow and green LEDs
  *portK = 0b00000111; // PK0-3 pull-up for buttons
  *ddrL  = 0b11111111; // ALL OUT for LCD (0-5), LEDs (6-7)
  *portL = 0b00000000; // NO pull-ups
  
  Wire.begin(); // for RTC
  SPI.begin(); // for RFID
  mfrc522.PCD_Init(); // Prepares RFID
  putChar('\n');
  if(!rtc.begin()){
    printEvent(1); // If RTC is malfunctioning/disconnected, display RTC ERROR to serial
  } else if(mfrc522.PCD_ReadRegister(mfrc522.VersionReg) == 0x00 || mfrc522.PCD_ReadRegister(mfrc522.VersionReg) == 0xFF){
    printEvent(2); // If RFID is malfunctioning/disconnected, post RFID ERROR to serial
  }

  if(!rtc.begin() || mfrc522.PCD_ReadRegister(mfrc522.VersionReg) == 0x00 || mfrc522.PCD_ReadRegister(mfrc522.VersionReg) == 0xFF){
    systemState = 3; // Immediately boot into ERROR state if RTC or RFID not found
  } else{
    printEvent(0); // If no errors, then boot normally into OFF state and post the state changed to serial
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Adjust clock to time at compilation
  lcd.begin(16, 2); // Prepares LCD
  lcd.print("SYSTEM INIT..."); // Initial LCD message

  // Configure interrupt
  *myPCICR |= (1 << PCIE2);
  *myPCMSK2 |= (1 << PCINT17);
  *myPCIFR |= (1 << PCIF2);
}

void loop() {
  switch(systemState){
    case 0: // OFF
      *portK &= ~((1 << 3) | (1 << 4)); // Turn off lights on K
      *portL &= ~((1 << 6) | (1 << 7)); // Turn off lights on L
      *portK |= (1 << 4); // Turn on WHITE

      if(lastState != systemState){ // If the state was just changed,
        lastState = 0; // set last state to current state,
        lcd.clear(); // clear the LCD, 
        printLCD("SYSTEM OFF", "COMPONENTS OK"); // print the state message,
        printEvent(3); // and print that the state is now OFF to serial.
      } // This only runs once.

      break;


    case 1: // IDLE
      *portK &= ~((1 << 3) | (1 << 4)); // Turn off lights on K
      *portL &= ~((1 << 6) | (1 << 7)); // Turn off lights on L
      *portK |= (1 << 3); // Turn on GREEN

      potValue = adc_read(0); // Read potentiometer value constantly
      if(potValue <= 300){ // and set the fare accordingly
        fareValue = 1;
      } else if(potValue <= 700){
        fareValue = 2;
      } else{
        fareValue = 3;
      }

      if(lastState != systemState){ // Similar to OFF state change check
        lastState = 1;
        noTone(12); // Stops buzzer in case last state was ERROR
        printEvent(4); // Post that the state is now IDLE to serial
        lcd.clear();
        if(fareValue == 1){ // This only runs when the state has just changed (in other words, it only runs once).
          printLCD("TAP CARD", "FARE: 1 CREDIT");
        } else if(fareValue == 2){
          printLCD("TAP CARD","FARE: 2 CREDITS");
        } else{
          printLCD("TAP CARD","FARE: 3 CREDITS");
        }
      }

      if(!(*pinK & (1 << PK0))){ // Change state to OFF when the OFF button is pressed
        systemState = 0;
      }
      
      currentMillis = millis();
      if(currentMillis - previousMillis >= interval){ // Update display to current fare every minute
        previousMillis = currentMillis;
        lcd.clear();
        if(fareValue == 1){
          printLCD("TAP CARD", "FARE: 1 CREDIT");
        } else if(fareValue == 2){
          printLCD("TAP CARD","FARE: 2 CREDITS");
        } else{
          printLCD("TAP CARD","FARE: 3 CREDITS");
        }
      }
    
      // RFID card ID check
      if(mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()){ // If card was detected
        if(isAuthorized(mfrc522.uid.uidByte, mfrc522.uid.size)){ // and the ID matches,
          printEvent(8); // print that a card was scanned to serial
          systemState = 2; // and continue to ACTIVE.
        } else{
          printEvent(7); // If the card is fraudulent (no ID match), print that an INVALID card was scanned to serial
          systemState = 3; // and continue to ERROR.
        }
      }

      break;


    case 2: // ACTIVE
      *portK &= ~((1 << 3) | (1 << 4)); // Turn off lights on K
      *portL &= ~((1 << 6) | (1 << 7)); // Turn off lights on L
      *portL |= (1 << 7); // Turn on YELLOW

      if(lastState != systemState){ // Similar to OFF/IDLE state change check
        lastState = 2;
        lcd.clear();
        printEvent(5); // Print that the state is now ACTIVE to serial
        success = chargeCard(fareValue); // changeCard is the RFID card logic function.
      }

      mfrc522.PICC_HaltA(); // Stop reading the card (so it only reads once)
      mfrc522.PCD_StopCrypto1();

      if(success == 1){ // If no problems occured during the card logic,
        systemState = 1; // continue to IDLE.
      } else{
        printEvent(10); // If there was a problem, print such
        systemState = 3; // and continue to ERROR.
      }

      break;


    case 3: // ERROR
      int prevState = lastState;
      *portK &= ~((1 << 3) | (1 << 4)); // Turn off lights on K
      *portL &= ~((1 << 6) | (1 << 7)); // Turn off lights on L
      *portL |= (1 << 6); // Turn on RED

      if(lastState != systemState){
        lcd.clear();
        printLCD("SYSTEM ERROR", "CHECK RFID, RTC");
        tone(12, 3000); // Play a nonstop tone
        printEvent(6); // Print an error state message to serial
        lastState = 3;
      }

      if(!(*pinK & (1 << PK2))){ // If the RESET button is pressed,
        printEvent(9); // state this in serial
        systemState = 1;
        mfrc522.PCD_Init(); // and reset RFID.
      }

      break;


  }
}

// ==============================================================================================================
// FUNCTIONS ----------------------------------------------------------------------------------------------------
// ==============================================================================================================

ISR(PCINT2_vect){ // ON button interrupt
  if (systemState == 0 && !(*pinK & (1 << PK1))) { // Only works when in "OFF" state and the ON button is pressed
    systemState = 1; // Returns the system to IDLE
  } 
}

// Timers -------------------------
void waitFor(unsigned int wait){
  unsigned long freq = 16000000;
  unsigned long prescaler = 64;
  // Reset registers
  *myTCCR1A = 0x00;
  *myTCCR1B = 0x00;
  // Reset counter
  *myTCNT1  = 0x00;

  // Configure count, start timer w/ 64 prescaler
  *myOCR1A = (freq / prescaler) / 1000;
  *myTCCR1B |= 0b00001011;

  for(unsigned int i = 0; i < wait; i++){
    while(!(*myTIFR1 & 0x02));
    *myTIFR1 |= 0x02;
  }

  // Stop timer
  *myTCCR1B = 0x00;

}

// ADC -------------------------
void adc_init(){
  // Enable ADC 
  *my_ADCSRA |= 0b10000000;
  // Disable ADC trigger mode
  *my_ADCSRA &= 0b11011111;
  // Disable ADC interrupt 
  *my_ADCSRA &= 0b11110111;
  // Set prescaler to read slowly
  *my_ADCSRA &= 0b11111000;
  // Reset channel and gain bits
  *my_ADCSRB &= 0b11110111;
  // Set free-running mode
  *my_ADCSRB &= 0b11111000;
  // Set up AVCC analog reference
  *my_ADMUX &= 0b11011111;
  *my_ADMUX &= 0b11100000;
  // Set result to right adjust
  *my_ADMUX &= 0b01111111;
   // Reset channel and gain bits
  *my_ADMUX |= 0b01000000;
}

unsigned int adc_read(unsigned char adc_channel_num){ // use channel 0
  // Clear channel selection bits
  *my_ADMUX &= 0b11100000;
  *my_ADCSRB &= 0b11110111;
  // Set channel selection bits for channel 0
  *my_ADMUX |= 0b0000000;
  // Start conversion
  *my_ADCSRA |= 0b01000000;
  // Wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // Return result in ADC register, formatted w/ right justification
  my_ADC_DATA = (unsigned short *) 0x78;
  unsigned int val = (*my_ADC_DATA & 0x03FF);
  return val;
}

// UART -------------------------
void U0init(int U0baud) {  // Start serial connection
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);
  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0 = tbaud;
}

// Reading from serial is ommitted.

void putChar(unsigned char U0pdata){ // Print a char to serial
  while (!(*myUCSR0A & (1 << 5)));
  *myUDR0 = U0pdata;
}

void printMulti(int value){
  putChar((value/10) + '0'); // print tens
  putChar((value%10) + '0'); // print ones
}

void printEvent(unsigned int eventCode){
  DateTime current = rtc.now();
  // Print date (MM-DD-YYYY)
  printMulti(current.month());
  putChar('/');
  printMulti(current.day());
  putChar('/');
  printMulti(current.year() / 100); // thousands, hundreds
  printMulti(current.year() % 100); // tens, ones
  putChar(' ');
  // Print time
  printMulti(current.hour());
  putChar(':');
  printMulti(current.minute());
  putChar(':');
  printMulti(current.second());
  // Add gap
  putChar(' ');
  putChar('-');
  putChar(' ');
  if(eventCode == 0){ // Good system start
    putChar('S'); // SYSTEM STARTED, NO ERRORS
    putChar('Y');
    putChar('S');
    putChar('T');
    putChar('E');
    putChar('M');
    putChar(' ');
    putChar('S');
    putChar('T');
    putChar('A');
    putChar('R');
    putChar('T');
    putChar('E');
    putChar('D');
    putChar(',');
    putChar(' ');
    putChar('N');
    putChar('O');
    putChar(' ');
    putChar('E');
    putChar('R');
    putChar('R');
    putChar('O');
    putChar('R');
    putChar('S');
  } else if(eventCode == 1){ // Bad system start - RTC error
    putChar('('); // (!!!) RTC ERROR
    putChar('!');
    putChar('!');
    putChar('!');
    putChar(')');
    putChar(' ');
    putChar('R');
    putChar('T');
    putChar('C');
    putChar(' ');
    putChar('E');
    putChar('R');
    putChar('R');
    putChar('O');
    putChar('R');
  } else if(eventCode == 2){ // Bad system start - RFID error
    putChar('('); // (!!!) RFID ERROR
    putChar('!');
    putChar('!');
    putChar('!');
    putChar(')');
    putChar(' ');
    putChar('R');
    putChar('F');
    putChar('I');
    putChar('D');
    putChar(' ');
    putChar('E');
    putChar('R');
    putChar('R');
    putChar('O');
    putChar('R');
  } else if(eventCode == 3){ // State change to OFF
    putChar('S'); // STATE OFF
    putChar('T');
    putChar('A');
    putChar('T');
    putChar('E');
    putChar(' ');
    putChar('O');
    putChar('F');
    putChar('F');
  } else if(eventCode == 4){ // State change to IDLE
    putChar('S'); // STATE IDLE
    putChar('T');
    putChar('A');
    putChar('T');
    putChar('E');
    putChar(' ');
    putChar('I');
    putChar('D');
    putChar('L');
    putChar('E');
  } else if(eventCode == 5){ // State change to ACTIVE
    putChar('S'); // STATE ACTIVE
    putChar('T');
    putChar('A');
    putChar('T');
    putChar('E');
    putChar(' ');
    putChar('A');
    putChar('C');
    putChar('T');
    putChar('I');
    putChar('V');
    putChar('E');
  } else if(eventCode == 6){ // State change to ERROR
    putChar('('); // (!!!) STATE ERROR
    putChar('!');
    putChar('!');
    putChar('!');
    putChar(')');
    putChar(' ');
    putChar('S');
    putChar('T');
    putChar('A');
    putChar('T');
    putChar('E');
    putChar(' ');
    putChar('E');
    putChar('R');
    putChar('R');
    putChar('O');
    putChar('R');
  } else if(eventCode == 7){ // Bad card ID
    putChar('('); // (!!!) INVALID CARD
    putChar('!');
    putChar('!');
    putChar('!');
    putChar(')');
    putChar(' ');
    putChar('I');
    putChar('N');
    putChar('V');
    putChar('A');
    putChar('L');
    putChar('I');
    putChar('D');
    putChar(' ');
    putChar('C');
    putChar('A');
    putChar('R');
    putChar('D');
  } else if(eventCode == 8){ // Good card ID
    putChar('C'); // CARD SCANNED
    putChar('A');
    putChar('R');
    putChar('D');
    putChar(' ');
    putChar('S');
    putChar('C');
    putChar('A');
    putChar('N');
    putChar('N');
    putChar('E');
    putChar('D');
  } else if(eventCode == 9){ // Reset pressed
    putChar('R'); // RESET PRESSED
    putChar('E');
    putChar('S');
    putChar('E');
    putChar('T');
    putChar(' ');
    putChar('P');
    putChar('R');
    putChar('E');
    putChar('S');
    putChar('S');
    putChar('E');
    putChar('D');
  } else if(eventCode == 10){ // Card read/write fail
    putChar('R'); // READ/WRITE FAIL
    putChar('E');
    putChar('A');
    putChar('D');
    putChar('/');
    putChar('W');
    putChar('R');
    putChar('I');
    putChar('T');
    putChar('E');
    putChar(' ');
    putChar('F');
    putChar('A');
    putChar('I');
    putChar('L');
  } else if(eventCode == 11){ // Charged 1 credit
    putChar('C'); // CHARGED 1 CREDIT
    putChar('H');
    putChar('A');
    putChar('R');
    putChar('G');
    putChar('E');
    putChar('D');
    putChar(' ');
    putChar('1');
    putChar(' ');
    putChar('C');
    putChar('R');
    putChar('E');
    putChar('D');
    putChar('I');
    putChar('T');
  } else if(eventCode == 12){ // Charged 2 credits
    putChar('C'); // CHARGED 2 CREDITS
    putChar('H');
    putChar('A');
    putChar('R');
    putChar('G');
    putChar('E');
    putChar('D');
    putChar(' ');
    putChar('1');
    putChar(' ');
    putChar('C');
    putChar('R');
    putChar('E');
    putChar('D');
    putChar('I');
    putChar('T');
    putChar('S');
  } else if(eventCode == 13){ // Charged 3 credits
    putChar('C'); // CHARGED 3 CREDITS
    putChar('H');
    putChar('A');
    putChar('R');
    putChar('G');
    putChar('E');
    putChar('D');
    putChar(' ');
    putChar('3');
    putChar(' ');
    putChar('C');
    putChar('R');
    putChar('E');
    putChar('D');
    putChar('I');
    putChar('T');
    putChar('S');
  } else if(eventCode == 14){ // Not enough credits
    putChar('I'); // INSUFFICIENT CREDITS
    putChar('N');
    putChar('S');
    putChar('U');
    putChar('F');
    putChar('F');
    putChar('I');
    putChar('C');
    putChar('I');
    putChar('E');
    putChar('N');
    putChar('T');
    putChar(' ');
    putChar('C');
    putChar('R');
    putChar('E');
    putChar('D');
    putChar('I');
    putChar('T');
    putChar('S');
  }

  // Newline
  putChar('\n');
}

// LCD -------------------------
void printLCD(const char* line1, const char* line2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
}

// RFID -------------------------
bool isAuthorized(byte *scannedUID, byte cardSize) { // Check if card ID is allowed
    byte masterCard[] = {0xE0, 0x50, 0xA8, 0x5F}; 

    for (byte i = 0; i < cardSize; i++) {
        if (scannedUID[i] != masterCard[i]) {
            return false; // Mismatch found
        }
    }
    return true; // All bytes matched
}

bool chargeCard(int amount){ // Return 0 for fail (card no write, etc.), 1 for success
  MFRC522::MIFARE_Key key;
  for(byte i = 0; i < 6; i++){key.keyByte[i] = 0xFF;}
  MFRC522::StatusCode status;
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &key, &(mfrc522.uid));
  if(status != MFRC522::STATUS_OK){ // If there is a read/write error,
    printEvent(10); // Print that there was to serial
    return; // If authentication fails, exit NOW!!!
  }
  // Create buffer for data
  byte buffer[18];
  byte size = sizeof(buffer);
  status = mfrc522.MIFARE_Read(4, buffer, &size);

  // Begin process
  if(status == MFRC522::STATUS_OK){
    int currentBalance = buffer[0];
    if(currentBalance >= amount){ // If enough credits,
      currentBalance -= amount; // charge the fare
      buffer[0] = (byte)currentBalance;
      mfrc522.MIFARE_Write(4, buffer, 16); // and write the new balance to the card.
      if(amount == 1){ // print to serial that...
        printEvent(11); // 1 credit was deducted
      } else if(amount == 2){
        printEvent(12); // 2 credits were deducted
      } else{
        printEvent(13); // 3 credits were deducted
      }
      lcd.clear(); // Print messages to LCD
      printLCD("WELCOME ABOARD.", "REMAINING: ");
      lcd.print(currentBalance);
      tone(12, 1047, 200); // Play good tone
      waitFor(200);
      tone(12, 1319, 200);
      waitFor(200);
      tone(12, 1568, 250);
      waitFor(1100);
      return true; // Exit
    } else{ 
      printEvent(14); // Print to serial that the card does not have enough credits
      lcd.clear();
      printLCD("NOT ENOUGH CREDS", "BAL: ");
      lcd.print(currentBalance);
      lcd.print(" | REQ: "); 
      lcd.print(amount);
      tone(12, 880, 200); // Play bad tone
      waitFor(200);
      tone(12, 698, 200);
      waitFor(200);
      tone(12, 523, 250);
      waitFor(3600);
      return true; // Exit
    }
    return false; // ONLY if there was a problem with read/write
  }
}
