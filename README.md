# Senior-Project-Code
Code for Senior project Spring 2026

Some things to do know before you put files in STM32 IDE
make sure you enable pin PB9 to I2C SDA and PB8 to I2C SCL
you need to make sure it enables I2C before you generate the code 

After you generate the code make you put the all the SSD.h files inside the Inc folder under core and all the SSD.c files under the Src folder


all the files starting with ssd are the library files to make the LCD screen work

I will also put another modified version of main that has the code I worked on instead of all the main code that STM32 provides since when it generates the code it produces a lot

You will also have enable float printing so we can display the float decimal values you do that by
  Right-click project → Properties
  Go to C/C++ Build → Settings → MCU GCC Linker → Miscellaneous
  In the Other flags box, add: -u _printf_float
  Click Apply and Close
  Clean the project, then Build and Flash again

Some notes on connections
For display and voltage and current sensors they share the same SDA and SCL lines (I2CA)
  SDA -> PB9
  SCL -> PB8

For display B that displays RPM on SDA and SCL lines (I2CB)
  SDA -> PA10
  SCL -> PA9

For screens:
  VCC -> 5V
  GND -> GND

For Voltage/Current sensor:
  VCC -> 3.3V
  GND -> GND
  Make sure GND is shared with load GND
  sensor has to be in series with what were trying to measure so:
    Vsource -> in+ (sensor) -> in- (sensor) -> load -> GND
    
For HAL effect sesnor
  VCC -> 5V or 3.3V
  GND -> GND
  S -> PB1

Extra pins:
A0: Sends 3.3V signal for Relay

# updates

3/24/26
Newest main.c includes reading the current and voltage from sensor and displaying the values will also have some codes in case current or voltage wont display so we can trouble shoot 

4/28/26
Added second I2C line which will be I2CB
Added 2nd Screen which will be on I2CB that reads the RPM measurement
Added pin for HAL effect sensor which reads magnet
Added calculations for RPM
Added GPIO output for A0 to send 3.3V signal for relay

