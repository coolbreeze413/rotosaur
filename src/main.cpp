/*

██████╗  ██████╗ ████████╗ ██████╗ ███████╗ █████╗ ██╗   ██╗██████╗ 
██╔══██╗██╔═══██╗╚══██╔══╝██╔═══██╗██╔════╝██╔══██╗██║   ██║██╔══██╗
██████╔╝██║   ██║   ██║   ██║   ██║███████╗███████║██║   ██║██████╔╝
██╔══██╗██║   ██║   ██║   ██║   ██║╚════██║██╔══██║██║   ██║██╔══██╗
██║  ██║╚██████╔╝   ██║   ╚██████╔╝███████║██║  ██║╚██████╔╝██║  ██║
╚═╝  ╚═╝ ╚═════╝    ╚═╝    ╚═════╝ ╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝


http://patorjk.com/software/taag/#p=display&f=ANSI%20Shadow&t=ROTOSAUR                           

*/


#include <Arduino.h>

/*
step 0 -> turns off
step 6 -> turns on, volume = 0
step 46 -> max, volume = 40
so, we have:
MIN_VOLUME -> 0
MAX_VOLUME -> 40

Note that, due to our mounting of the stepper: 
V+ -> single step ANTICLOCKWISE upto MAX_VOLUME
V- -> single step CLOCKWISE upto MIN_VOLUME
MUTE -> "current_volume" steps CLOCKWISE (will end up at MIN_VOLUME)
UNMUTE -> "current_volume" steps ANTICLOCKWISE (will start from MIN_VOLUME)
A -> POWER OFF or POWER ON
B+ / C- -> SINGLE STEP ANTICLOCKWISE or CLOCKWISE, no LIMIT for recovery/debugging
D -> RESET internal state to default: powered off + not muted + volume is MIN_VOLUME

on turn on ROTASAUR, we assume we are at default state as above.
this can be updated to use the EEPROM based readback instead.
EEPROM write-cycles of 100000 is what we need to think about.
possibility: save previous states on POWER_OFF, restore on POWER_ON -> is_muted, and current_volume
Keep this as  TODO.

-------------------------------------------------------------
actions/verbs:
do power_on -> turn POWER_NUM_STEPS ANTICLOCKWISE
do power_off -> state can be muted, not muted
    if muted -> turn POWER_NUM_STEPS CLOCKWISE
    if not muted -> turn ("current_volume" + POWER_NUM_STEPS) CLOCKWISE

do_mute -> turn "current_volume" steps clockwise
do_unmute -> turn "current_volume" steps anticlockwise

vol + -> turn single step anticlockwise upto MAX_VOLUME
vol - -> turn single step clockwise upto MIN_VOLUME

--------------------------------------------------------------
states/nouns:
is_powered = true/false
is_muted = true/false
curent_volume = ?

is_powered = false, -> only power on allowed
is_powered = true ?
    is_muted = true, -> only unmute -OR- power off allowed

*/
int current_volume = 0;
bool is_muted = false;
bool is_powered = false;
unsigned long last_change_time_ms = 0;
unsigned long last_anti_burn_cycle_ms = 0;
bool is_anti_burn_in_active = false;
uint8_t anti_burn_in_cycle_type = 0;
uint32_t num_anti_burn_cycles_completed = 0;
#define LOOP_DELAY_ms                               300
#define POWER_NUM_STEPS                             6
#define MIN_VOLUME                                  0
#define MAX_VOLUME                                  40  
#define OLED_BURN_IN_THRESHOLD_ms                   5000
#define OLED_BURN_IN_CYCLE_ms                       5000
#define OLED_NUM_ANTI_BURN_IN_CYCLE_TYPES           4
#define OLED_TURN_OFF_THRESHOLD_BURN_IN_CYCLES      180     // 15 min = 900000ms/5000ms = 180 cycles of anti-burn-in


#include <IRremote.h>
#define RECV_PIN                11
IRrecv irrecv(RECV_PIN);
decode_results results;
#define SAMSUNG_VOL_UP          0xE0E0E01F
#define SAMSUNG_VOL_DOWN        0xE0E0D02F
#define SAMSUNG_MUTE            0xE0E0F00F
#define SAMSUNG_A               0xE0E036C9
#define SAMSUNG_B               0xE0E028D7
#define SAMSUNG_C               0xE0E0A857
#define SAMSUNG_D               0xE0E06897


#include <CheapStepper.h>
#define ULN2003_IN1_PIN         4
#define ULN2003_IN2_PIN         5
#define ULN2003_IN3_PIN         6
#define ULN2003_IN4_PIN         7
CheapStepper stepperC(ULN2003_IN1_PIN, ULN2003_IN2_PIN, ULN2003_IN3_PIN, ULN2003_IN4_PIN);
#define NUM_VOLUME_DIVISIONS    32
#define TOTAL_NUM_DIVISIONS     4096
#define SINGLE_STEP (TOTAL_NUM_DIVISIONS / NUM_VOLUME_DIVISIONS)


//#define SSD1306_ADAFRUIT_135_13_LIB
#define SSD1306_GREIMAN_5169_LIB

#ifdef SSD1306_ADAFRUIT_135_13_LIB
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH            128 // OLED display width, in pixels
#define SCREEN_HEIGHT           32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET              -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define LOGO_HEIGHT             16
#define LOGO_WIDTH              16
static const unsigned char PROGMEM logo_bmp[] =
    {
        B00000000, B11000000,
        B00000001, B11000000,
        B00000001, B11000000,
        B00000011, B11100000,
        B11110011, B11100000,
        B11111110, B11111000,
        B01111110, B11111111,
        B00110011, B10011111,
        B00011111, B11111100,
        B00001101, B01110000,
        B00011011, B10100000,
        B00111111, B11100000,
        B00111111, B11110000,
        B01111100, B11110000,
        B01110000, B01110000,
        B00000000, B00110000
    };
#endif // #ifdef SSD1306_ADAFRUIT_135_13_LIB

#ifdef SSD1306_GREIMAN_5169_LIB
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
// 0x3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS                 0x3C
// Define proper RST_PIN if required.
#define RST_PIN                     -1
SSD1306AsciiWire oled;
#endif // #ifdef SSD1306_GREIMAN_5169_LIB


void do_power()
{
    // power up sequence :
    if(is_powered == false)
    {
        if(is_muted == true)
        {
            // was muted, so only power on to zero volume
            stepperC.move(false, ((POWER_NUM_STEPS) * (SINGLE_STEP)));
        }
        else
        {
            // was not muted, so power on to previously active volume
            stepperC.move(false, ((POWER_NUM_STEPS + current_volume) * (SINGLE_STEP)));
        }
        
        is_powered = true;        
    }
    // power down sequence :
    else
    {
        if(is_muted == true)
        {
            // we are muted, so only power off from zero volume                
            stepperC.move(true, ((POWER_NUM_STEPS)* (SINGLE_STEP)));
        }
        else
        {
            // we are not muted so power off from currently active volume
            stepperC.move(true, ((POWER_NUM_STEPS + current_volume) * (SINGLE_STEP)));
        }
        is_powered = false;
    }
}


void do_mute()
{
    // ignore mute when powered off or at zero volume
    if(is_powered == false) return;
    if(current_volume == MIN_VOLUME) return;
    
    if(is_muted == false)
    {
        // go from current volume to zero
        stepperC.move(true, ((current_volume) * (SINGLE_STEP)));
        is_muted = true;
    }
    else
    {
        // go from zero to current volume
        stepperC.move(false, ((current_volume) * (SINGLE_STEP)));
        is_muted = false;
    }

    //Serial.print("MUTE -> ");Serial.println(is_muted);    
}


#define ARG_VOLUME_UP           true
#define ARG_VOLUME_DOWN         false
void do_volume(bool up)
{
    // ignore volume commands when powered off or when muted
    // actually, when muted, vol+ generally works, to unmute, then do a vol+. TODO: REQUIRED?
    // when muted, should vol- work as well ? TODO: REQUIRED?
    if(is_powered == false) return;
    if(is_muted == true) return;

    if(up == true)
    {
        if (current_volume >= MAX_VOLUME) return;

        stepperC.move(false, SINGLE_STEP);
        current_volume++;
        //Serial.print("VOL_UP -> ");Serial.println(current_volume);
    }
    else
    {
        if (current_volume <= MIN_VOLUME) return;

        stepperC.move(true, SINGLE_STEP);
        current_volume--;
        //Serial.print("VOL_DOWN -> ");Serial.println(current_volume);
    }    
}


void updateDisplay()
{
    // maybe the display is currently off in the anti-burn-in cycle
    oled.ssd1306WriteCmd(SSD1306_DISPLAYON);

    // maybe the display is currently inverted in the anti-burn-in cycle
    oled.invertDisplay(false);

    // maybe the display was not in 2x mode in the anti-burn-in cycle
    oled.set2X();

    // clean slate
    oled.clear();    

    if(is_powered == false)
    {
        oled.print("OFF");
    }
    else if ( (is_muted == true) || (current_volume == 0) )
    {
        oled.print("[ 0_0]");
    }
    else if (current_volume == MAX_VOLUME)
    {
        oled.print("MAX");
    }
    else
    {
        oled.print("[ ");
        if (current_volume < 10)
        {
            oled.print("0");
        }
        oled.print(current_volume);
        oled.println(" ]");
    }
}


void showSplashScreen() 
{
    oled.clear();
    oled.print("[  R");delay(100);oled.print("O"); delay(100); oled.print("T"); delay(100);oled.println("O  ]");delay(100);
    oled.print("[  S");delay(100);oled.print("A");delay(100);oled.print("U");delay(100);oled.print("R  ]");delay(500);
    oled.invertDisplay(true);delay(500);
    oled.invertDisplay(false);delay(500);
    oled.invertDisplay(true);delay(500);
    oled.invertDisplay(false);
}


void do_OLEDAntiBurnIn()
{
    // to prevent the OLED from burning in, we need to periodically change the display contents
    // so that the same text does not continue to be displayed for a long time.

    // so, after a user-triggered change (IR remote button press), we do:
    // - update display according to currently active state (off/mute/volume)
    // - disable anti-burn-in mode
    // - and wait for a threshold, of say 10 seconds to kick into anti-burn-in mode for the OLED.
    // once we are in anti-burn-in mode, we cycle the display contents every X duration, 
    // which we may say is the oled anti-burn-in-cycle time. we will cycle between:
    // 1. display the splash screen style for X duration
    // 2. turn off display for X duration
    // 3. display the current state (as normal) for X duration
    // 4. turn off display for X duration
    // cycle back to 1
    // again, after a long idle time of our anti-burn-in mode, we switch off the display completely.

    if (is_anti_burn_in_active == false)
    {
        last_change_time_ms += LOOP_DELAY_ms;
        if (last_change_time_ms > OLED_BURN_IN_THRESHOLD_ms)
        {
            // kick in the anti burn in mode:
            is_anti_burn_in_active = true;

            // init the burn cycle timer
            // init this to OLED burn in cycle time instead of zero, so that we kick in the first
            // burn in cycle immediately, on the next loop. yeah, it is a bit disingenuous.
            last_anti_burn_cycle_ms = OLED_BURN_IN_CYCLE_ms;

            // start from the first anti-burn-in cycle
            anti_burn_in_cycle_type = 0;

            // reset count of number of anti burn in cycles done
            num_anti_burn_cycles_completed = 0;
        }
    }
    else // anti-burn in mode is active already
    {
        // idle for more than x hours, switch off the display completely, wake up when user interacts (ir-receive)
        if(num_anti_burn_cycles_completed > OLED_TURN_OFF_THRESHOLD_BURN_IN_CYCLES)
        {
            oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
        }               
        else if(last_anti_burn_cycle_ms >= OLED_BURN_IN_CYCLE_ms)
        {            
            // execute the current burn in cycle
            if(anti_burn_in_cycle_type == 0)
            {
                // display logo screen
                oled.ssd1306WriteCmd(SSD1306_DISPLAYON);
                oled.clear();
                oled.set1X();
                oled.println("[  ROTO  ]");
                oled.println("[  SAUR  ]");
                oled.invertDisplay(true);
            }
            else if(anti_burn_in_cycle_type == 1)
            {
                // turn off display
                oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
            }
            else if(anti_burn_in_cycle_type == 2)
            {
                // display current state
                updateDisplay();
            }
            else if(anti_burn_in_cycle_type == 3)
            {
                // turn off display
                oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
            }

            // go to the next anti burn oled cycle
            anti_burn_in_cycle_type++;
            if(anti_burn_in_cycle_type == OLED_NUM_ANTI_BURN_IN_CYCLE_TYPES)
            {
                anti_burn_in_cycle_type = 0;
            }
            last_anti_burn_cycle_ms = 0;

            // keep track of the "idle" cycles of doing just burn-in
            num_anti_burn_cycles_completed++;
        }

        // increment the anti-burn-in cycle timer
        last_anti_burn_cycle_ms += LOOP_DELAY_ms;
    }
    
}


void saveStateToEEPROM()
{
    // is_muted, current_volume
    // save only at every power_off cycle ?
}


void restoreStateFromEEPROM()
{
    // is_muted, current_volume
    // restore at first boot only
}


void setup()
{
    //pinMode(LED_PIN, OUTPUT); // Defining LED pin as OUTPUT Pin.
    //digitalWrite(LED_PIN, 0);

    Serial.begin(9600);


    // In case the interrupt driver crashes on setup, give a clue
    // to the user what's going on.
    Serial.println("Init InfraRed");
    irrecv.enableIRIn(); // Start the receiver
    Serial.println();

    Serial.println("Init Stepper");
    stepperC.setRpm(10);
    Serial.print(stepperC.getRpm()); // get the RPM of the stepper
    Serial.print(" rpm = delay of ");
    Serial.print(stepperC.getDelay()); // get delay between steps for set RPM
    Serial.println(" microseconds between steps");
    Serial.println();


    Serial.println("Init OLED");
#ifdef SSD1306_ADAFRUIT_135_13_LIB
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    { // Address 0x3D for 128x64
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }

    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.display();
    //delay(2000); // Pause for 2 seconds

    // Clear the buffer
    //display.clearDisplay();
#endif // #ifdef SSD1306_ADAFRUIT_135_13_LIB

#ifdef SSD1306_GREIMAN_5169_LIB
    Wire.begin();
    Wire.setClock(400000L);

#if RST_PIN >= 0
    oled.begin(&Adafruit128x64, I2C_ADDRESS, RST_PIN);
#else  // RST_PIN >= 0
    oled.begin(&Adafruit128x64, I2C_ADDRESS);
#endif // RST_PIN >= 0

    oled.setFont(Cooper26);
    showSplashScreen();
    updateDisplay();

#endif // #ifdef SSD1306_GREIMAN_5169_LIB
    Serial.println();

    Serial.println("Ready");
    Serial.println();
}


void loop()
{
    if (irrecv.decode(&results))
    {
        Serial.println(results.value, HEX);

        if(results.value == SAMSUNG_VOL_UP)
        {
            do_volume(ARG_VOLUME_UP);
        }
        else if(results.value == SAMSUNG_VOL_DOWN)
        {
            do_volume(ARG_VOLUME_DOWN);
        }
        else if(results.value == SAMSUNG_MUTE)
        {
            do_mute();
        }
        else if (results.value == SAMSUNG_A) 
        {
            do_power();
        }
        else if (results.value == SAMSUNG_B) 
        {
            // debug, do vol+ without affecting state
            Serial.println("B +");
            stepperC.move(false, SINGLE_STEP);
        }
        else if (results.value == SAMSUNG_C) 
        {
            // debug, do vol- without affecting state
            Serial.println("C -");
            stepperC.move(true, SINGLE_STEP);
        }
        else if (results.value == SAMSUNG_D) 
        {
            // debug, reset our state machine and variables to complete power off
            Serial.println("D");
            is_powered = false;
            is_muted = false;
            current_volume = 0;
        }       
        else
        {
            Serial.println("unhandled IR code");            
        }


        // this will reset the OLED anti burn in mode
        is_anti_burn_in_active = false;

        // we have received an IR code, so user is active, indicate by setting last change time        
        last_change_time_ms = 0;

        // update the display according to current state
        updateDisplay();


        Serial.print("is_powered: ");Serial.println(is_powered);
        Serial.print("is_muted: ");Serial.println(is_muted);
        Serial.print("volume: ");Serial.println(current_volume);
        Serial.print("step position: ");Serial.print(stepperC.getStep());Serial.println(" / 4096");
        Serial.println();


        irrecv.resume(); // IR ready to receive the next value
    }

    // loop delay is required, so that we have a sane amount of IR codes received
    // the remote might send the code multiple times, as those membrane keys are 
    // notorious for their mushy multi press behavior
    delay(LOOP_DELAY_ms);

    // check and perform OLED anti burn techniques if required
    do_OLEDAntiBurnIn();
}