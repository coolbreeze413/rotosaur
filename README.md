# ROTOSAUR(us)
Arduino based IR remote controller for old speakers with mechanical knob-controlled volume, using a stepper.

```
██████╗  ██████╗ ████████╗ ██████╗ ███████╗ █████╗ ██╗   ██╗██████╗ 
██╔══██╗██╔═══██╗╚══██╔══╝██╔═══██╗██╔════╝██╔══██╗██║   ██║██╔══██╗
██████╔╝██║   ██║   ██║   ██║   ██║███████╗███████║██║   ██║██████╔╝
██╔══██╗██║   ██║   ██║   ██║   ██║╚════██║██╔══██║██║   ██║██╔══██╗
██║  ██║╚██████╔╝   ██║   ╚██████╔╝███████║██║  ██║╚██████╔╝██║  ██║
╚═╝  ╚═╝ ╚═════╝    ╚═╝    ╚═════╝ ╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝
```

Thanks: http://patorjk.com/software/taag/#p=display&f=ANSI%20Shadow&t=ROTOSAUR

```
    SCHEMATIC:

+     +--------+ A4   SDA +---------+ VDD +
+-----+        +----------+         +-----+
      |        |          | SSD1306 |
      |        | A5   SCL | I2C     |
      |        +----------+         +-----+
      | ATMEGA |          +---------+ GND -
      | 328P   |
      |        |
      |        | D4   IN1 +---------+ VDD +
      |        +----------+         +-----+
      |        |          | ULN2003 |
      |        | D5   IN2 |         |
      |        +----------+ STEPPER |
      |        |          | CONTROL |
      |        | D6   IN3 |         |
      |        +----------+         |
      |        |          |         |
      |        | D7   IN4 |         |
      |        +----------+         +-----+
      |        |          +---------+ GND -
      |        |
      |        |
      |        | D11    Y +---------+ R   +
      |        +----------+         +-----+
      |        |          | IR      |
+-----+        |          |         +-----+
-     +--------+          +---------+ G   -


```

## Hardware

1. Arduino Nano (ATMega328P) :
    - https://robu.in/product/arduino-nano-v3-0-ch340-chip-mini-usb-cable/

2. ULN2003 Stepper Motor Controller : 
    - https://robu.in/product/uln2003-driver-module-stepper-motor-driver/

3. 28BYJ-48 Stepper Motor : 
    - https://www.digibay.in/4-phase-5-wire-stepper-motor-5v-28byj-48-5v

4. Belt Drive (or O-Ring) :
    - https://robu.in/product/gt2-close-loop-188mm-long-6mm-width-rubber-timing-belt-for-3d-printer/

5. Stepper Motor Timing Pulley 5mm Bore GT2 Profile : 
     - https://robu.in/product/aluminum-gt2-timing-pulley-for-6mm-belt-20-tooth-5mm-bore-2pcs/

6. Foam Tape to put over the knob so that we have enough friction for the timing belt to rotate the knob without slippage and wasted torque

7. 2 Sided Perf-Board to mount them all + Wires + Female Headers etc.

8. Transparent Plastic Box to hold them all.


## NOTES


```
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

```
