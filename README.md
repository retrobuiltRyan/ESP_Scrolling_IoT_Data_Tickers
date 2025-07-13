# ESP_Scrolling_IoT_Data_Tickers
ESP8266 (or ESP32) IoT data tickers for news, weather, stocks. For those classic looking 8x8 LED matrix modules. Custom ESP8266 Node MCU 12-E PCB (just bc i have a bunch of these dev boards I need to put to use to toss) with built-in buck converter (6-28V in) and SPI headers. PCB could use one more rev to address some quirks with the ESP8266's strapping pins (rotary encoder), but project is consider 'finished'.  
https://youtu.be/6R5AQ15R8a4

API's used:
api.weather.gov
https://newsapi.org/
finnhub.io

<img width="1920" height="1080" alt="vlcsnap-2025-07-13-12h41m03s127" src="https://github.com/user-attachments/assets/b098f0cc-9393-42ed-935b-0f0a0a135597" />

![PXL_20250623_163714544](https://github.com/user-attachments/assets/e7ffc85a-02b7-469b-b387-6faad6f4ef6c)

![P1110438](https://github.com/user-attachments/assets/886af54f-24a8-4bbb-9fbd-8fce82c25473)

<img width="1432" height="649" alt="PCB" src="https://github.com/user-attachments/assets/aa618138-f004-412f-bd7a-feac1086d5c7" />

| Reference          | Value                      | Footprint                                                                 | Qty | DNP  | DigiKey P/N             |
|--------------------|----------------------------|---------------------------------------------------------------------------|-----|------|--------------------------|
| BZ1                | Buzzer_5V                  | Buzzer_Beeper:MagneticBuzzer_ProSignal_ABT-410-RC                         | 1   | DNP     |                          |
| C1,C3,C4,C5,C6     | 0.1uF 50V                  | Capacitor_SMD:C_1206_3216Metric                                           | 5   |      | 1276-1068-1-ND           |
| C2,C7              | 0.22uF 50V CER             | Capacitor_SMD:C_1206_3216Metric                                           | 2   |      | 445-2283-1-ND            |
| D8                 | D                          | Diode_SMD:D_SMC_Handsoldering                                            | 1   | DNP  | S5AC-FDICT-ND            |
| D9                 | SMF15A                     | Diode_SMD:D_SMF                                                           | 1   |      | SMF15A-E3-08CT-ND        |
| J1                 | Barrel_Jack                | Connector_BarrelJack:BarrelJack_GCT_DCJ200-10-A_Horizontal               | 1   |      | EJ508A-ND                |
| J2,J5,J6,J10       | Screw_Terminal_2_P3.50mm   | TerminalBlock_Phoenix:PT-1,5-2-3.5-H_1x02_P3.50mm_Horizontal              | 4   |      |732-2747-ND|
| J3                 | PINHD_1x5_Male             | Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical               | 1   |      |  generic 0.1" breakaway header                        |
| J8                 | PINHD_1x15_Male            | Connector_PinHeader_2.54mm:PinHeader_1x15_P2.54mm_Vertical               | 1   |      |   generic 0.1" breakaway header |
| Q1                 | MOSFET P-CH 30V 25A TO252  | MOSFET_P-CH_30V_25A_TO252                                                | 1   |      | 785-1106-1-ND            |
| Q2                 | MMBT2222A                  | Package_TO_SOT_SMD:SOT-23                                                 | 1   |      | MMBT2222ATPMSCT-ND       |
| R1                 | 220                        | Resistor_SMD:R_1206_3216Metric                                            | 1   |      | 311-220FRCT-ND           |
| R2,R3,R5,R6        | 1K                         | Resistor_SMD:R_1206_3216Metric                                            | 4   |      | 311-1.00KFRCT-ND         |
| R4                 | 3.3K                       | Resistor_SMD:R_1206_3216Metric                                            | 1   |      | 311-1.00KFRCT-ND         |
| R7                 | 100k                       | Resistor_SMD:R_1206_3216Metric                                            | 1   |      | 311-100KFRCT-ND          |
| R8,R9,R10          | 10K                        | Resistor_SMD:R_1206_3216Metric                                            | 3   |      | 311-10.0KFRCT-ND         |
| SW1                | RotaryEncoder_Switch_MP    | Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm            | 1   | DNP     | PEC11R-4220F-S0024-ND    |
| U1                 | MAX40200AUK                | Package_TO_SOT_SMD:SOT-23-5                                               | 1   |      | 175-MAX40203AUK+TCT-ND   |
| U2                 | MP1584_Buck                | MP1584                                                                    | 1   |    [Aliexpress](https://www.aliexpress.us/item/3256806890547813.html)                         |
| U3                 | L7805                      | Package_TO_SOT_SMD:TO-252-2                                               | 1   |DNP      | 497-7255-1-ND            |




