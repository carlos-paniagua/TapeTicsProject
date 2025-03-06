# TapeTics

## Introduction
This repository contains the hardware and software design files for TapeTics, which is currently under review for presentation at [IEEE VR 2025](https://ieeevr.org/2025/).

## Content
[Controller_Data](https://github.com/carlos-paniagua/TapeTics/tree/main/Controller_Data)/ - Controller Board Circuit Information \
[Tape_Data](https://github.com/carlos-paniagua/TapeTics/tree/main/Tape_Data)/ - Circuit Information on Tape Board\
[Library](https://github.com/carlos-paniagua/TapeTics/tree/main/Library)/ - Library files for Arduino IDE\
[TapeTicsCode](https://github.com/carlos-paniagua/TapeTics/tree/main/TapeTicsCode)/ - Arduino IDE .ino file\
[GUI](https://github.com/carlos-paniagua/TapeTics/tree/main/GUI)/ - GUI controller developed in Max8\
[PythonCode](https://github.com/carlos-paniagua/TapeTics/tree/main/PythonCode)/ - Used in conjunction with GUI

## Software Version Information
1. Arduino IDE 2.3.2\
   Queue 2,0\
   NimBLE-Arduino 1.4.2\
   FastLED 3.7.3\
   M5Stack 2.1.1
2. Max 8.6.4
3. Python 3.12\
   Bleak \
   pythonosc
4. VScode


## Cost per Node

| Parts                         |   Quantity | Value              |   Cost  ($) |
|:------------------------------|-----------:|:-------------------|-----------------:|
| Chip Capacitor 0.1μf |3 | [CC0603KRX7R9BB104](https://jlcpcb.com/partdetail/Yageo-CC0603KRX7R9BB104/C14663)  |0.0063|
| Chip Capacitor 10μf|2 | [CL10A106KP8NNNC](https://jlcpcb.com/partdetail/20411-CL10A106KP8NNNC/C19702) |0.01|
| Diode|1 | [1N4001W](https://jlcpcb.com/partdetail/Yongyutai-1N4001W/C2944152)|0.0052|
| RGB-LED  |  1 | [WS2812B-V5/W](https://jlcpcb.com/partdetail/Worldsemi-WS2812B_V5W/C2874885)       |0.0762|
| Neopixel IC| 1 | [WS2811](https://jlcpcb.com/partdetail/Worldsemi-WS2811/C114581)             |0.033|
| Vibrating Motor|1 | [LCM1027A2445F](https://jlcpcb.com/partdetail/Leader-LCM1027A2445F/C2759984)      |0.30|
| Chip Resistor 33Ω| 2 | [0603WAF330JT5E](https://jlcpcb.com/partdetail/23867-0603WAF330JT5E/C23140)     |0.002|
| Chip Resistor 100Ω |  1 | [0603WAF1000T5E](https://jlcpcb.com/partdetail/23502-0603WAF1000T5E/C22775)    |0.001 |
| Photocoupler |1 | [TLP627(TP1,F)](https://jlcpcb.com/partdetail/Toshiba-TLP627_TP1_F/C30336)      | 0.48 |
| Voltage Regulator 16V to 5V| 1 | [SPX3819M5-L-5-0/TR](https://jlcpcb.com/partdetail/MaxLinear-SPX3819M5_L_5_0TR/C13417) |0.1463|
| Voltage Regulator 16V to 3.3V  |1 | [SPX3819M5-L-3-3/TR](https://jlcpcb.com/partdetail/Maxlinear-SPX3819M5_L_3_3TR/C9055) |0.0828|

<!-- |||Parts Total Cost|1.1428|
|||Parts Assenbly Cost|115.63|
|||PCB Cost|115.63| -->
