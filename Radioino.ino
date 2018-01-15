// \file Radioino.ino
// \brief FM Radio implementation using RDA5807M, Verstärker, LCD 1602, TTP224.
//
// \author WeWeWe - Welle West Wetterau e.V.
// \copyright Copyright (c) 2018 by WeWeWe - Welle West Wetterau e.V.\n
// This work is licensed under "t.b.d.".\n
//
// \details
// This is a full function radio implementation that uses a LCD display to show the current station information.\n
// Its using a Arduino Nano for controling other chips using I2C
//
// Depencies:
// ----------
// aka the giant shoulders we stand on
// https://github.com/mathertel/Radio
// https://github.com/mathertel/OneButton
// https://github.com/marcoschwartz/LiquidCrystal_I2C
//
// Wiring:
// -------
// t.b.d.
//
// Userinterface:
// --------------
// +----------------+---------------------+-----------------+---------------------+---------------------------+
// | Button \ Event |       click()       | double click()  |     long press      |          comment          |
// +----------------+---------------------+-----------------+---------------------+---------------------------+
// | VolDwn         | -1 volume           | toggle mute     | each 500ms -1       | if vol eq min then mute   |
// | VolUp          | +1 volume           | toggle loudness | each 500ms +1       | if muted the first unmute |
// | R3We           | tune 87.8 MHz       |                 | scan to next sender |                           |
// | Disp           | toggle display mode |                 |                     | > freq > radio > audio >  |
// +----------------+---------------------+-----------------+---------------------+---------------------------+
//
// History:
// --------
// * 02.01.2018 created.

#include <radio.h>
#include <RDA5807M.h>
#include <RDSParser.h>
#include <OneButton.h>
#include <LiquidCrystal_I2C.h>


// - - - - - - - - - - - - - - - - - - - - - - //
//  Globals
// - - - - - - - - - - - - - - - - - - - - - - //
//Buttons
#define BUTTON_VOLDOWN 2
#define BUTTON_VOLUP   3
#define BUTTON_R3WE    4
#define BUTTON_DISP    5

OneButton buttVolUp(BUTTON_VOLUP, true);
OneButton buttVolDown(BUTTON_VOLDOWN, true);
OneButton buttR3We(BUTTON_R3WE, true);
OneButton buttDisp(BUTTON_DISP, true);

// Define some stations available at your locations here:
// 89.40 MHz as 8940
RADIO_FREQ preset[] = {
  8780,  // * WeWeWe
  8930,  // * hr3
  9440,  // * hr1
  10590, // * FFH
  10660  // * Radio Bob
};
int    i_sidx = 2;

// What to Display
// on any change don't forget to change ++ operator below and lastDisp variable in updateLCD
enum DISPLAY_STATE {
  TEXT,     // radio text (RDS)
  FREQ,     // station freqency
  TIME,     // time (RDS)
  AUDIO,    // Volume info, muted, stereo/mono, boost
  SIG       // signalinfo SNR RSSI
} displayState = 0;

// Overload ++ operator, for cylcling trough
inline DISPLAY_STATE& operator++(DISPLAY_STATE& eDOW, int) {
  const int i = static_cast<int>(eDOW) + 1;
  eDOW = static_cast<DISPLAY_STATE>((i) % 5); //Need to be changed if enum type is changes
  return eDOW;
}

// RDS Time container
struct {
  uint8_t hour = NULL;
  uint8_t minute = NULL;
} rdsTime;

// RDS Text container
String rdsText = " ";

// Create an instance of a RDA5807 chip radio
RDA5807M radio;

// Create an instance for RDS info parsing
RDSParser rds;

// Create an instance for LCD 16 chars and 2 line display
// LCD address may vary
LiquidCrystal_I2C lcd(0x27, 16, 2);

// - - - - - - - - - - - - - - - - - - - - - - //
//  functions and callbacks
// - - - - - - - - - - - - - - - - - - - - - - //

// display volume on change
void volume() {
  lcd.setCursor(0, 1);
  lcd.print("Vol:" );
  lcd.print(radio.getVolume());
  delay(300);
} //volume

// callback for RDS parser by radio
void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) {
  rds.processData(block1, block2, block3, block4);
} //RDS_process

// callback pdate the ServiceName text on the LCD display
void DisplayServiceName(char *name) {
  lcd.setCursor(0, 0); lcd.print("        "); // clear 8 chars
  lcd.setCursor(0, 0); lcd.print(name);
} // DisplayServiceName

// callback update on RDS time
void UpdateRDSTime(uint8_t h, uint8_t m) {
  rdsTime.hour = h;
  rdsTime.minute = m;
} // UpdateRDSTime

// callback for update on RDS text
void UpdateRDSText(char *text) {
  rdsText = String(text);
} // UpdateTDSText

// callback for volume down
void VolDown() {
  int v = radio.getVolume();
  if (v > 0) radio.setVolume(--v);
  else if (v == 0) radio.setSoftMute(true);
  volume();
} //VolDown

// callback for volume up
void VolUp() {
  int v = radio.getVolume();
  if (v < 15) radio.setVolume(++v);
  else if (v >= 0) radio.setSoftMute(false);
  volume();
} //VolUp

// callback for toggle mute
void Mute() {
  radio.setSoftMute(!radio.getSoftMute());
} //Mute

// callback for toggle boost
void Boost() {
  radio.setBassBoost(!radio.getBassBoost());
} //Boost

// callback for toggle Display
void Display() {
  displayState++;
} //Display

// callback for R3We Button click
void R3We() {
  radio.setFrequency(preset[0]);
} //R3We


// - - - - - - - - - - - - - - - - - - - - - - //
//  Display updater
// - - - - - - - - - - - - - - - - - - - - - - //
// Update LCD based displayState
void updateLCD() {
  static RADIO_FREQ lastfreq = 0;
  static uint8_t lastmin = 0;
  static uint8_t rdsTexti = 0;
  static DISPLAY_STATE lastDisp = 0;
  lcd.setCursor(9, 0);
  RADIO_INFO info;
  AUDIO_INFO ainfo;
  radio.getRadioInfo(&info);
  radio.getAudioInfo(&ainfo);
  info.tuned      ? lcd.print("T ") : lcd.print("  ");  // print "T" if station tuned
  info.stereo     ? lcd.print("S ") : lcd.print("  ");  // print "S" if tuned stereo
  ainfo.softmute  ? lcd.print("M ") : lcd.print("  ");  // print "M" if muted
  ainfo.bassBoost ? lcd.print("B")  : lcd.print(displayState);   // print "B" if loundness is ON
  if (displayState != lastDisp) {
    //erase Diplay if state changed
    lastDisp = displayState;
    lcd.setCursor(0, 1);
    lcd.print("                 ");
  }
  lcd.setCursor(0, 1);
  switch (displayState) {
    case FREQ: {
        RADIO_FREQ freq = radio.getFrequency();
        if (freq != lastfreq) {
          lastfreq = freq;
          char s[12];
          radio.formatFrequency(s, sizeof(s));
          lcd.print("Freq: "); lcd.print(s);
        }
        break;
      }
    case TEXT: {
        lcd.print(rdsText.substring(rdsTexti, rdsTexti + 15));
        (rdsText.length() > rdsTexti + 15) ? rdsTexti++ : rdsTexti = 0;
        break;
      }
    case AUDIO: {
        lcd.print("Audio not impl.");
      }
    case TIME: {
        if (rdsTime.hour != NULL ) {
          if (rdsTime.minute != lastmin) {
            lastmin = rdsTime.minute;
            String s;
            s = "      ";
            if (rdsTime.hour < 10) s += "0";
            s += rdsTime.hour;
            s += ":";
            if (rdsTime.minute < 10) s += "0";
            s += rdsTime.minute;
            s += "     ";
            lcd.print(s);
          }
        }
        else {
          lcd.print("NO RDS DATE/TIME");
        }
        break;
      }
    case SIG: {
        String s;
        s = "SNR: ";
        s += info.snr;
        s += " RSSI: ";
        s += info.rssi;
        s += "   ";
        lcd.print(s);
        break;
      }
    default:
      break; // do nothing
  } //switch
} //updateLCD

//Display A greeting Message :)
void displayGreetings() {
  byte s2[8] = { 0b00000,
                 0b00001,
                 0b00001,
                 0b00011,
                 0b00011,
                 0b00011,
                 0b00011,
                 0b00011
               };
  byte s3[8] = { 0b11111,
                 0b11111,
                 0b11111,
                 0b11110,
                 0b00000,
                 0b00000,
                 0b00000,
                 0b00000
               };
  byte s4[8] = { 0b00011,
                 0b00111,
                 0b01100,
                 0b01101,
                 0b01101,
                 0b01100,
                 0b00111,
                 0b00011
               };
  byte s5[8] = { 0b10011,
                 0b11011,
                 0b01011,
                 0b11011,
                 0b10011,
                 0b00111,
                 0b11110,
                 0b11000
               };
  byte s6[8] = { 0b11100,
                 0b11100,
                 0b11100,
                 0b11100,
                 0b11100,
                 0b11100,
                 0b11100,
                 0b11100
               };
  //lcd.createChar(0, s0);
  //lcd.createChar(1, s1);
  lcd.createChar(2, s2);
  lcd.createChar(3, s3);
  lcd.createChar(4, s4);
  lcd.createChar(5, s5);
  lcd.createChar(6, s6);
  lcd.clear();
  lcd.blink();
  lcd.cursor();
  lcd.backlight();
  lcd.setCursor(1, 0); lcd.write((uint8_t) 2); lcd.write((uint8_t) 3);
  lcd.print(" *RADIOINO*");
  lcd.setCursor(0, 1); lcd.write((uint8_t) 4); lcd.write((uint8_t) 5);
  lcd.setCursor(4, 1);
  String s = "Welle West";
  for (byte i = 0; i < 11; i++) {
    lcd.print(s.substring(i, i + 1));
    delay(200);
  }
  delay(500);
  for (byte i = 13; i > 2; i--) {
    lcd.setCursor(i, 1);
    lcd.print(" ");
    delay(100);
  }
  s = "*Wetterau* ";
  for (byte i = 0; i < 12; i++) {
    lcd.print(s.substring(i, i + 1));
    delay(200);
  }
  delay(2000);
  lcd.noBlink();
  lcd.noCursor();
  lcd.clear();
} //displayGreetings


// - - - - - - - - - - - - - - - - - - - - - - //
//  Setup and initialisations
// - - - - - - - - - - - - - - - - - - - - - - //
void setup() {
  // Initialize the Display
  lcd.init();
  displayGreetings();

  // Initialize the Radio
  radio.init();
  radio.setBandFrequency(RADIO_BAND_FM, preset[i_sidx]);
  radio.setMono(false);
  radio.setMute(false);
  radio.setVolume(1);
  // setup the information chain for RDS data.
  radio.attachReceiveRDS(RDS_process);
  rds.attachServicenNameCallback(DisplayServiceName); // callback for rds programmname
  rds.attachTextCallback(UpdateRDSText); // callback for rds radio text
  rds.attachTimeCallback(UpdateRDSTime); // callback for rds time

  // Configure our userinterface
  // Buttons
  pinMode(BUTTON_VOLDOWN, INPUT); // VolDown
  pinMode(BUTTON_VOLUP, INPUT);   // VolUp
  pinMode(BUTTON_R3WE, INPUT);    // WeWeWe
  pinMode(BUTTON_DISP, INPUT);    // Display
  //Callback for Buttons
  buttVolUp.attachClick(VolUp);
  buttVolUp.attachDoubleClick(Boost);
  //  buttVolUp.attachDuringLongPress();
  buttVolDown.attachClick(VolDown);
  buttVolDown.attachDoubleClick(Mute);
  //  buttVolDown.attachDuringLongPress();
  //  buttR3We.attachClick(R3We);
  //  buttR3We.attachDoubleClick();
  //  buttR3We.attachLongPressStart();
  buttDisp.attachClick(Display);
  //  buttDisp.attachDoubleClick();
  //  buttDisp.attachLongPressStart();
} //setup


// - - - - - - - - - - - - - - - - - - - - - - //
//  main loop
// - - - - - - - - - - - - - - - - - - - - - - //
void loop() {
  unsigned long now = millis();
  static unsigned long nextDispTime = 0;
  static unsigned long nextRDSTime = 0;
  static unsigned long nextTick = 0;

  // Check Buttons
  if (now > nextTick) {
    buttVolUp.tick();
    buttVolDown.tick();
    buttR3We.tick();
    buttDisp.tick();
    nextTick = now + 5;
  }

  // check for RDS data
  if (now > nextRDSTime) {
    radio.checkRDS();
    nextRDSTime = now + 50;
  }
  // update the display from time to time
  if (now > nextDispTime) {
    updateLCD();
    nextDispTime = now + 500;
  }
} //loop