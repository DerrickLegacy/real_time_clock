Hardware Setup Checklist
Devices Used 
    1. Aduino Uno with Atmega328P
    2. Real Time Clock Module DS1302

DS1302 Connections:
    VCC to 5V (with backup battery)
    GND to ground (Any GND Port on Aduino) 
    RST to PB2 (10 on Aduino) 
    SCLK to PB1(9 on Aduino)    
    I/O to PB0 (8 on Aduino) 

Power Considerations:
    DS1302 needs a backup battery (3V coin cell)
    Main power to Arduino should be 5V