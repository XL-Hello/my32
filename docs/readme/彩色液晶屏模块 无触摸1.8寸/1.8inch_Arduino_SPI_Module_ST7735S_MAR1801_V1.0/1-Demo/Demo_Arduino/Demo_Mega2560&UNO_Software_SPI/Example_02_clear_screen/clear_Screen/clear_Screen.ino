// IMPORTANT: LCDWIKI_SPI LIBRARY MUST BE SPECIFICALLY
// CONFIGURED FOR EITHER THE TFT SHIELD OR THE BREAKOUT BOARD.

//This program is a demo of clearing screen to display black,white,red,green,blue.

//Set the pins to the correct ones for your development shield or breakout board.
//when using the BREAKOUT BOARD only and using these software spi lines to the LCD,
//You can use any free pin to define the pins,
//if you don't need to control the LED pin,you can set it to 3.3V and set the pin definition to -1.
//for example
//pin usage as follow:
//                      CS   RS   RST   SDA   CLK   VCC   GND    
//Arduino Mega2560&Uno  A5   A3   A4    A2    A1    5V    GND

//Remember to set the pins to suit your display module!
/********************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, QD electronic SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE 
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
**********************************************************************************/
#include <LCDWIKI_GUI.h> //Core graphics library
#include <LCDWIKI_SPI.h> //Hardware-specific library

//paramters define
#define MODEL ST7735S
#define CS   A5
#define CD   A3
#define RST  A4
#define SDA  A2
#define SCK  A1
#define LED  -1   //if you don't need to control the LED pin,you should set it to -1 and set it to 3.3V

//the definiens of software spi mode as follow:
//if the IC model is known or the modules is unreadable,you can use this constructed function
LCDWIKI_SPI mylcd(MODEL,CS,CD,-1,SDA,RST,SCK,LED); //model,cs,dc,sdo,sda,reset,sck,led

void setup() 
{
    mylcd.Init_LCD();
    mylcd.Fill_Screen(0x0000);
    mylcd.Fill_Screen(0xFFFF);
}

void loop() 
{    
    mylcd.Fill_Screen(0,0,0);
    mylcd.Fill_Screen(255,255,255);
    mylcd.Fill_Screen(255,0,0); 
    mylcd.Fill_Screen(0,255,0);
    mylcd.Fill_Screen(0,0,255);
    delay(3000);
    mylcd.Fill_Screen(0,0,0);
    delay(1000);
    mylcd.Fill_Screen(255,255,255);
    delay(1000);
    mylcd.Fill_Screen(0xF800);
    delay(1000);
    mylcd.Fill_Screen(0x07E0);
   delay(1000);
   mylcd.Fill_Screen(0x001F);
   delay(3000);
}
