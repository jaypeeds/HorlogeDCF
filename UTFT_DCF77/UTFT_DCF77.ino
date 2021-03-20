//
//  www.blinkenlight.net
//
//  Copyright 2016 Udo Klein
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see http://www.gnu.org/licenses/

#include <dcf77.h>

// Affichage TFT
#include <TFT_ST7735.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#define TFT_GREY 0x7BEF
#define TFT_W 160
#define TFT_H 128

// Buffers pour affichage
char fdate[16];
char ftime[9];

TFT_ST7735 myGLCD = TFT_ST7735(); // Invoke library, pins defined in User_Setup.h

#define DELAY 500

unsigned long runTime = 0;

#if defined(__AVR__)
const uint8_t dcf77_analog_sample_pin = 5;
//const uint8_t dcf77_sample_pin = A5;    // A5 == d19
const uint8_t dcf77_sample_pin = 2; // Any valid digital pin
const uint8_t dcf77_inverted_samples = 0;
const uint8_t dcf77_analog_samples = 0;
const uint8_t dcf77_pin_mode = INPUT; // disable internal pull up
// const uint8_t dcf77_pin_mode = INPUT_PULLUP;  // enable internal pull up

const uint8_t dcf77_monitor_led = 18; // A4 == d18

uint8_t ledpin(const uint8_t led)
{
    return led;
}
#else
const uint8_t dcf77_sample_pin = 53;
const uint8_t dcf77_inverted_samples = 0;

// const uint8_t dcf77_pin_mode = INPUT;  // disable internal pull up
const uint8_t dcf77_pin_mode = INPUT_PULLUP; // enable internal pull up

const uint8_t dcf77_monitor_led = 19;

uint8_t ledpin(const uint8_t led)
{
    return led < 14 ? led : led + (54 - 14);
}
#endif

// DCF transmitter is in CET/CEST time zone
const int8_t timezone_offset = 0; // Paris, Amsterdam, Berlin, Roma...

namespace Timezone
{
    uint8_t days_per_month(const Clock::time_t &now)
    {
        switch (now.month.val)
        {
        case 0x02:
            // valid till 31.12.2399
            // notice year mod 4 == year & 0x03
            return 28 + ((now.year.val != 0) && ((bcd_to_int(now.year) & 0x03) == 0) ? 1 : 0);
        case 0x01:
        case 0x03:
        case 0x05:
        case 0x07:
        case 0x08:
        case 0x10:
        case 0x12:
            return 31;
        case 0x04:
        case 0x06:
        case 0x09:
        case 0x11:
            return 30;
        default:
            return 0;
        }
    }

    void adjust(Clock::time_t &time, const int8_t offset)
    {
        // attention: maximum supported offset is +/- 23h

        int8_t hour = BCD::bcd_to_int(time.hour) + offset;

        if (hour > 23)
        {
            hour -= 24;
            uint8_t day = BCD::bcd_to_int(time.day) + 1;
            if (day > days_per_month(time))
            {
                day = 1;
                uint8_t month = BCD::bcd_to_int(time.month);
                ++month;
                if (month > 12)
                {
                    month = 1;
                    uint8_t year = BCD::bcd_to_int(time.year);
                    ++year;
                    if (year > 99)
                    {
                        year = 0;
                    }
                    time.year = BCD::int_to_bcd(year);
                }
                time.month = BCD::int_to_bcd(month);
            }
            time.day = BCD::int_to_bcd(day);
            time.weekday.val = time.weekday.val < 7 ? time.weekday.val + 1 : time.weekday.val - 6;
        }

        if (hour < 0)
        {
            hour += 24;
            uint8_t day = BCD::bcd_to_int(time.day) - 1;
            if (day < 1)
            {
                uint8_t month = BCD::bcd_to_int(time.month);
                --month;
                if (month < 1)
                {
                    month = 12;
                    int8_t year = BCD::bcd_to_int(time.year);
                    --year;
                    if (year < 0)
                    {
                        year = 99;
                    }
                    time.year = BCD::int_to_bcd(year);
                }
                time.month = BCD::int_to_bcd(month);
                day = days_per_month(time);
            }
            time.day = BCD::int_to_bcd(day);
            time.weekday.val = time.weekday.val > 1 ? time.weekday.val - 1 : time.weekday.val + 6;
        }

        time.hour = BCD::int_to_bcd(hour);
    }
}

uint8_t sample_input_pin()
{
    const uint8_t sampled_data =
#if defined(__AVR__)
        dcf77_inverted_samples ^ (dcf77_analog_samples ? (analogRead(dcf77_analog_sample_pin) > 200)
                                                       : digitalRead(dcf77_sample_pin));
#else
        dcf77_inverted_samples ^ digitalRead(dcf77_sample_pin);
#endif

    digitalWrite(ledpin(dcf77_monitor_led), sampled_data);
    return sampled_data;
}

void dateTimeDisplay(Clock::time_t now)
{
    char *fmtDate = formatDate(now);
    char *fmtTime = formatTime(now);
    char *fmtTimeZone = formatTimeZone(now);

    myGLCD.setTextColor(TFT_MAGENTA, TFT_BLACK);
    myGLCD.drawString(fmtDate, 30, 32, 2);
    myGLCD.setTextColor(TFT_CYAN, TFT_BLACK);
    myGLCD.drawString(fmtTime, 32, 56, 4);
    myGLCD.drawString(fmtTimeZone, 65, 82, 2);   
}

Clock::time_t dummy;
void makeDummyDateTime() {
  dummy.day = BCD::int_to_bcd(1);  
  dummy.month = BCD::int_to_bcd(12);
  dummy.year = BCD::int_to_bcd(41);
  dummy.weekday = BCD::int_to_bcd(4);
  dummy.hour = BCD::int_to_bcd(1);  
  dummy.minute = BCD::int_to_bcd(23);
  dummy.second = BCD::int_to_bcd(45);
  dummy.uses_summertime = true;
}

const char *fDayOfWeek(Clock::time_t now)
{
    switch (now.weekday.val)
    {
    case 0x01:
        return "LUN";
    case 0x02:
        return "MAR";
    case 0x03:
        return "MER";
    case 0x04:
        return "JEU";
    case 0x05:
        return "VEN";
    case 0x06:
        return "SAM";
    case 0x07:
        return "DIM";
    }
}

const char *fMonth(Clock::time_t now)
{
    switch (now.month.val)
    {
    case 0x01:
        return "JAN";
    case 0x02:
        return "FEV";
    case 0x03:
        return "MAR";
    case 0x04:
        return "AVR";
    case 0x05:
        return "MAI";
    case 0x06:
        return "JUN";
    case 0x07:
        return "JUL";
    case 0x08:
        return "AOU";
    case 0x09:
        return "SEP";
    case 0x10:
        return "OCT";
    case 0x11:
        return "NOV";
    case 0x12:
        return "DEC";
    }
}

char *formatDate(Clock::time_t now) {
  int offset = 0;
  strncpy(fdate, fDayOfWeek(now), 3);
  fdate[3] = ' ';
  if (now.day.digit.hi) {
    fdate[4] = '0' + now.day.digit.hi;
    fdate[5] = '0' + now.day.digit.lo;
    offset = 6;  
  } else {
    fdate[4] = '0' + now.day.digit.lo;
    offset = 5;
  }
  fdate[offset] = ' ';
  strncpy(fdate + offset + 1, fMonth(now), 3);
  fdate[offset + 4] = ' ';
  fdate[offset + 5] = '2';
  fdate[offset + 6] = '0';
  fdate[offset + 7] = '0' + now.year.digit.hi;
  fdate[offset + 8] = '0' + now.year.digit.lo;
  fdate[offset + 9] = 0;
  return fdate;
}

char *formatTime(Clock::time_t now) {
  ftime[0] = '0' + now.hour.digit.hi;
  ftime[1] = '0' + now.hour.digit.lo;
  ftime[2] = ':';
  ftime[3] = '0' + now.minute.digit.hi;
  ftime[4] = '0' + now.minute.digit.lo;
  ftime[5] = ':';
  ftime[6] = '0' + now.second.digit.hi;
  ftime[7] = '0' + now.second.digit.lo;
  ftime[8] = 0;    
  return ftime;
}

char *formatTimeZone(Clock::time_t now)
{
  return now.uses_summertime ? " CEST" : " CET ";
}

void setup()
{
    pinMode(7, OUTPUT);
    digitalWrite(7, LOW);
    delay(10);
    digitalWrite(7, HIGH);
    // Setup the LCD
    myGLCD.init();
    myGLCD.setRotation(3);

    myGLCD.fillScreen(TFT_BLACK);
    myGLCD.setTextColor(TFT_WHITE, TFT_BLACK);
    myGLCD.drawString("* DCF77 (C) 2016 U.Klein", 4, 8, 1);

    pinMode(ledpin(dcf77_monitor_led), OUTPUT);
    pinMode(dcf77_sample_pin, dcf77_pin_mode);

    DCF77_Clock::setup();
    DCF77_Clock::set_input_provider(sample_input_pin);

    // Placeholder date & time
    makeDummyDateTime();
    dateTimeDisplay(dummy);
    
    // Wait till clock is synced, depending on the signal quality this may take
    // rather long. About 5 minutes with a good signal, 30 minutes or longer
    // with a bad signal
    for (uint8_t state = Clock::useless;
         state == Clock::useless || state == Clock::dirty;
         state = DCF77_Clock::get_clock_state())
    {

        // wait for next sec
        Clock::time_t now;
        DCF77_Clock::get_current_time(now);
        // render one dot per second while initializing
        static uint8_t count = 0;
        myGLCD.drawString((count++ % 2) ? "|" : "-", 4, 8, 1);
    }
    myGLCD.drawString(" ", 4, 8, 1);
}

void loop()
{
  Clock::time_t now;
  DCF77_Clock::get_current_time(now);
  Timezone::adjust(now, timezone_offset);
  dateTimeDisplay(now);   
}
