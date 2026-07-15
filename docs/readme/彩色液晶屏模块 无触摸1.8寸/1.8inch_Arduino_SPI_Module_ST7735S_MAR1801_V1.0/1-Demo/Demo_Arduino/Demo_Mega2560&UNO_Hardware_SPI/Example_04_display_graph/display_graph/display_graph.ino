// IMPORTANT: LCDWIKI_SPI LIBRARY MUST BE SPECIFICALLY
// CONFIGURED FOR EITHER THE TFT SHIELD OR THE BREAKOUT BOARD.

//This program is a demo of how to display picture and 
//how to use rotate function to display string.

//when using the BREAKOUT BOARD only and using these hardware spi lines to the LCD,
//the SDA pin and SCK pin is defined by the system and can't be modified.
//if you don't need to control the LED pin,you can set it to 3.3V and set the pin definition to -1.
//other pins can be defined by youself,for example
//pin usage as follow:
//                  CS   RS    RST   SDI   CLK   VCC   GND
//Arduino Mega2560  A5   A3    A4    51    52    5V    GND
//Arduino Uno       A5   A3    A4    11    13    5V    GND

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
#define LED  -1   //if you don't need to control the LED pin,you should set it to -1 and set it to 3.3V

//the definiens of hardware spi mode as follow:
//if the IC model is known or the modules is unreadable,you can use this constructed function
LCDWIKI_SPI mylcd(MODEL,CS,CD,RST,LED); //model,cs,dc,reset,led

#define  BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

void fill_screen_test()
{
  mylcd.Fill_Screen(BLACK);  
  mylcd.Fill_Screen(RED);
  mylcd.Fill_Screen(GREEN);
  mylcd.Fill_Screen(BLUE);
  mylcd.Fill_Screen(BLACK);
}

void text_test()
{
  mylcd.Set_Text_Mode(0); 
  mylcd.Set_Text_Back_colour(BLACK);
  mylcd.Set_Text_colour(WHITE);
  mylcd.Set_Text_Size(1);
  mylcd.Print_String("Hello World!", 0, 0);
  
  mylcd.Set_Text_colour(YELLOW);
  mylcd.Set_Text_Size(2);
  mylcd.Print_Number_Float(1234.56,2,0, 8, '.', 0, ' ');
  
  mylcd.Set_Text_colour(RED);
  mylcd.Set_Text_Size(3);
  mylcd.Print_Number_Int(0xDEADBEF, 0, 24, 0, ' ', 16); 

  mylcd.Set_Text_colour(GREEN);
  mylcd.Set_Text_Size(4);  
  mylcd.Print_String("Groop", 0, 48); 

  mylcd.Set_Text_Size(2);
  mylcd.Print_String("I implore,", 0, 80);

  mylcd.Set_Text_Size(1);
  mylcd.Print_String("my foonting turling.", 0, 96);
  mylcd.Print_String("And hooptiously me", 0, 104);
  mylcd.Print_String("with crinkly bindle,", 0, 112);
  mylcd.Print_String("Or I will rend thee", 0, 120);
}

void lines_test(void)
{
    mylcd.Fill_Screen(BLACK);
      mylcd.Set_Draw_color(GREEN);
    int i = 0;   
    for(i = 0; i< mylcd.Get_Display_Width();i+=5)
    {
       mylcd.Draw_Line(0, 0, i, mylcd.Get_Display_Height()-1);
     }
     for(i = mylcd.Get_Display_Height()-1; i>= 0;i-=5)
     {
       mylcd.Draw_Line(0, 0, mylcd.Get_Display_Width()-1, i);
     }
     
     mylcd.Fill_Screen(BLACK); 
       mylcd.Set_Draw_color(RED);
    for(i = mylcd.Get_Display_Width() -1; i>=0;i-=5)
    {
      mylcd.Draw_Line(mylcd.Get_Display_Width()-1, 0, i, mylcd.Get_Display_Height()-1);
     }
    for(i = mylcd.Get_Display_Height()-1; i>=0;i-=5)
    {
      mylcd.Draw_Line(mylcd.Get_Display_Width()-1, 0, 0, i);
     }
     
     mylcd.Fill_Screen(BLACK); 
      mylcd.Set_Draw_color(BLUE);
     for(i = 0; i < mylcd.Get_Display_Width();i+=5)
    {
      mylcd.Draw_Line(0, mylcd.Get_Display_Height()-1, i, 0);
     }
     for(i = 0; i < mylcd.Get_Display_Height();i+=5)
    {
      mylcd.Draw_Line(0, mylcd.Get_Display_Height()-1, mylcd.Get_Display_Width()-1, i);
     }

     mylcd.Fill_Screen(BLACK);
      mylcd.Set_Draw_color(YELLOW);
     for(i = mylcd.Get_Display_Width()-1; i >=0;i-=5)
    {
      mylcd.Draw_Line(mylcd.Get_Display_Width()-1, mylcd.Get_Display_Height()-1, i, 0);
     }
     for(i = 0; i<mylcd.Get_Display_Height();i+=5)
    {
      mylcd.Draw_Line(mylcd.Get_Display_Width()-1, mylcd.Get_Display_Height()-1, 0, i);
     }
}

void h_l_lines_test(void)
{
    int i=0;
   mylcd.Fill_Screen(BLACK);
     mylcd.Set_Draw_color(GREEN);
    for(i =0;i<mylcd.Get_Display_Height();i+=5)
    {
      mylcd.Draw_Fast_HLine(0,i,mylcd.Get_Display_Width()); 
      delay(5);
    }
     mylcd.Set_Draw_color(BLUE);
     for(i =0;i<mylcd.Get_Display_Width();i+=5)
    {
      mylcd.Draw_Fast_VLine(i,0,mylcd.Get_Display_Height()); 
           delay(5);
    }
}

void rectangle_test(void)
{
  int i = 0;
   mylcd.Fill_Screen(BLACK);
     mylcd.Set_Draw_color(GREEN);
   for(i = 0;i<mylcd.Get_Display_Width()/2;i+=4)
   {
      mylcd.Draw_Rectangle(i,(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2+i,mylcd.Get_Display_Width()-1-i,mylcd.Get_Display_Height()-(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2-i);  
        delay(5);
   }
}

void fill_rectangle_test(void)
{
  int i = 0;
   mylcd.Fill_Screen(BLACK);
     mylcd.Set_Draw_color(YELLOW);
   mylcd.Fill_Rectangle(0,(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2,mylcd.Get_Display_Width()-1,mylcd.Get_Display_Height()-(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2);
    mylcd.Set_Draw_color(MAGENTA);
   for(i = 0;i<mylcd.Get_Display_Width()/2;i+=4)
   {
      mylcd.Draw_Rectangle(i,(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2+i,mylcd.Get_Display_Width()-1-i,mylcd.Get_Display_Height()-(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2-i);  
        delay(5);
   }
   for(i = 0;i<mylcd.Get_Display_Width()/2;i+=4)
   {
       mylcd.Set_Draw_color(random(255), random(255), random(255));
      mylcd.Fill_Rectangle(i,(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2+i,mylcd.Get_Display_Width()-1-i,mylcd.Get_Display_Height()-(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2-i);  
        delay(5);
   }
}

void fill_circles_test(void)
{
  int r=10,i=0,j=0;
  mylcd.Fill_Screen(BLACK);
   mylcd.Set_Draw_color(MAGENTA);
  for(i=r;i<mylcd.Get_Display_Width();i+=2*r)
  {
    for(j=r;j<mylcd.Get_Display_Height();j+=2*r)
    {
      mylcd.Fill_Circle(i, j, r);
    }
  }
}

void circles_test(void)
{
  int r=10,i=0,j=0;
   mylcd.Set_Draw_color(GREEN);
  for(i=0;i<mylcd.Get_Display_Width()+r;i+=2*r)
  {
    for(j=0;j<mylcd.Get_Display_Height()+r;j+=2*r)
    {
      mylcd.Draw_Circle(i, j, r);
    }
  }  
}

void triangles_test(void)
{
   int i = 0;
   mylcd.Fill_Screen(BLACK);
   for(i=0;i<mylcd.Get_Display_Width()/2;i+=5)
   {
      mylcd.Set_Draw_color(0,i+64,i+64);
      mylcd.Draw_Triangle(mylcd.Get_Display_Width()/2-1,mylcd.Get_Display_Height()/2-1-i,
                    mylcd.Get_Display_Width()/2-1-i,mylcd.Get_Display_Height()/2-1+i,
                    mylcd.Get_Display_Width()/2-1+i,mylcd.Get_Display_Height()/2-1+i);                   
   }
}

void fill_triangles_test(void)
{
   int i = 0;
   mylcd.Fill_Screen(BLACK);
    for(i=mylcd.Get_Display_Width()/2-1;i>0;i-=5)
   {
    
      mylcd.Set_Draw_color(0,i+64,i+64);
      mylcd.Fill_Triangle(mylcd.Get_Display_Width()/2-1,mylcd.Get_Display_Height()/2-1-i,
                    mylcd.Get_Display_Width()/2-1-i,mylcd.Get_Display_Height()/2-1+i,
                    mylcd.Get_Display_Width()/2-1+i,mylcd.Get_Display_Height()/2-1+i);                   
      mylcd.Set_Draw_color(i,0,i);
      mylcd.Draw_Triangle(mylcd.Get_Display_Width()/2-1,mylcd.Get_Display_Height()/2-1-i,
                    mylcd.Get_Display_Width()/2-1-i,mylcd.Get_Display_Height()/2-1+i,
                    mylcd.Get_Display_Width()/2-1+i,mylcd.Get_Display_Height()/2-1+i);                   
   }
}

void round_rectangle(void)
{
   int i = 0;
   mylcd.Fill_Screen(BLACK);
     for(i = 0;i<mylcd.Get_Display_Width()/2;i+=4)
   {
       mylcd.Set_Draw_color(255-i,0,160-i);
      mylcd.Draw_Round_Rectangle(i,(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2+i,mylcd.Get_Display_Width()-1-i,mylcd.Get_Display_Height()-(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2-i,8);
        delay(5);
   } 
}

void fill_round_rectangle(void)
{
     int i = 0;
   mylcd.Fill_Screen(BLACK);
     for(i = 0;i<mylcd.Get_Display_Width()/2;i+=4)
   {
       mylcd.Set_Draw_color(255-i,160-i,0);
      mylcd.Fill_Round_Rectangle(i,(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2+i,mylcd.Get_Display_Width()-1-i,mylcd.Get_Display_Height()-(mylcd.Get_Display_Height()-mylcd.Get_Display_Width())/2-i,8);
        delay(5);
   } 
}

void setup() 
{
  mylcd.Init_LCD();
  fill_screen_test();
  delay(500);
  text_test();
  delay(500);
  lines_test();
  delay(500);
  h_l_lines_test();
  delay(500);
  rectangle_test();
  delay(500);
  fill_rectangle_test();
  delay(500);
  fill_circles_test();
  delay(500);
  circles_test();
  delay(500);
  triangles_test();
  delay(500);
  fill_triangles_test();
  delay(500);
   round_rectangle();
   delay(500);
   fill_round_rectangle();
   delay(3000);
}

void loop() 
{
 for(uint8_t rotation=0; rotation<4; rotation++) 
 {
    mylcd.Fill_Screen(BLACK); 
    mylcd.Set_Rotation(rotation);
    text_test();
    delay(2000);
 }
}
