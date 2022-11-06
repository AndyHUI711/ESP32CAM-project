// MCUFRIEND UNO shields have microSD on pins 10, 11, 12, 13
// The official <SD.h> library only works on the hardware SPI pins
// e.g. 11, 12, 13 on a Uno
// e.g. 50, 51, 52 on a Mega2560
//
// For not using a UNO,  must use Software SPI:
//
// NEW: install <SdFat.h> v2.x.x library with the Arduino Library Manager
// NEW: edit the src/SdFatConfig.h file to #define SPI_DRIVER_SELECT 2
//


#include <SPI.h>             // f.k. for Arduino-1.5.2
#define USE_SDFAT
#include <SdFat.h>           // Use the SdFat library

#if SPI_DRIVER_SELECT != 2
#error edit SdFatConfig.h .  READ THE SKETCH INSTRUCTIONS
#endif

SoftSpiDriver<12, 11, 13> softSpi; //Bit-Bang on the Shield pins SDFat.h v2
SdFat SD;
#define SD_CS SdSpiConfig(10, DEDICATED_SPI, SD_SCK_MHZ(0), &softSpi)

#include <Adafruit_GFX.h>    // Hardware-specific library
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;

#define NAMEMATCH "logo"         // "" matches any name

#define PALETTEDEPTH   8     // support 256-colour Palette

#include <TouchScreen.h>
#define MINPRESSURE 200
#define MAXPRESSURE 1000

// ALL Touch panels and wiring is DIFFERENT
// copy-paste results from TouchScreen_Calibr_native.ino
const int XP = 6, XM = A2, YP = A1, YM = 7; //ID=0x9341
const int TS_LEFT = 907, TS_RT = 136, TS_TOP = 942, TS_BOT = 139;

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

Adafruit_GFX_Button on_btn, off_btn;

int pixel_x, pixel_y;     //Touch_getXY() updates global vars
bool Touch_getXY(void)
{
    TSPoint p = ts.getPoint();
    pinMode(YP, OUTPUT);      //restore shared pins
    pinMode(XM, OUTPUT);
    digitalWrite(YP, HIGH);   //because TFT control pins
    digitalWrite(XM, HIGH);
    bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
    if (pressed) {
        pixel_x = map(p.x, TS_LEFT, TS_RT, 0, tft.width()); //.kbv makes sense to me
        pixel_y = map(p.y, TS_TOP, TS_BOT, 0, tft.height());
    }
    return pressed;
}

#define BLACK   0x0000
#define RED     0xF800
#define GREEN   0x07E0
#define WHITE   0xFFFF
#define BLUE    0x001F
#define GREY    0x8410
#define ORANGE  0xF3C0
#define YELLOW  0xF764
#define CYAN    0x07FF


#include "Fonts/FreeSans6pt7b.h"
#include "Fonts/FreeSans9pt7b.h"
#include "Fonts/FreeSans12pt7b.h"

char namebuf[32] = "/";   //BMP files in root directory
//char namebuf[32] = "/bitmaps/";  //BMP directory e.g. files in /bitmaps/*.bmp
#define PI 3.1415926535897932384626433832795


File root;
int pathlen;
int col[8];

#include <SimpleDHT.h> 
#include <MQ135.h>
#include <Wire.h>
#include <BH1750.h> 

#define watersensor A15
#define airSensor A14
#define soilMoisture A13

int pinDHT11 = 53;
SimpleDHT11 dht11;
MQ135 gasSensor = MQ135(airSensor);
BH1750 lightMeter;
int lux;
int relayPin =51; //继电器
int LED = 40;

void showmsgXY(int x, int y, int sz, const GFXfont *f, const char *msg, const char *color)
{
  int16_t x1, y1;
  uint16_t wid, ht;
  tft.setFont(f);
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.setTextSize(sz);
  tft.print(msg);
}


#define BMPIMAGEOFFSET 54

#define BUFFPIXEL      20

void waterPump(){
  digitalWrite(relayPin, HIGH);
  delay(1000);
  digitalWrite(relayPin, LOW);
}

void stopPump(){
  digitalWrite(relayPin, LOW);
}

uint16_t read16(File& f) {
    uint16_t result;         // read little-endian
    f.read(&result, sizeof(result));
    return result;
}

uint32_t read32(File& f) {
    uint32_t result;
    f.read(&result, sizeof(result));
    return result;
}

uint8_t showBMP(char *nm, int x, int y)
{
    File bmpFile;
    int bmpWidth, bmpHeight;    // W+H in pixels
    uint8_t bmpDepth;           // Bit depth (currently must be 24, 16, 8, 4, 1)
    uint32_t bmpImageoffset;    // Start of image data in file
    uint32_t rowSize;           // Not always = bmpWidth; may have padding
    uint8_t sdbuffer[3 * BUFFPIXEL];    // pixel in buffer (R+G+B per pixel)
    uint16_t lcdbuffer[(1 << PALETTEDEPTH) + BUFFPIXEL], *palette = NULL;
    uint8_t bitmask, bitshift;
    boolean flip = true;        // BMP is stored bottom-to-top
    int w, h, row, col, lcdbufsiz = (1 << PALETTEDEPTH) + BUFFPIXEL, buffidx;
    uint32_t pos;               // seek position
    boolean is565 = false;      //

    uint16_t bmpID;
    uint16_t n;                 // blocks read
    uint8_t ret;

    if ((x >= tft.width()) || (y >= tft.height()))
        return 1;               // off screen

    bmpFile = SD.open(nm);      // Parse BMP header
    bmpID = read16(bmpFile);    // BMP signature
    (void) read32(bmpFile);     // Read & ignore file size
    (void) read32(bmpFile);     // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile);       // Start of image data
    (void) read32(bmpFile);     // Read & ignore DIB header size
    bmpWidth = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    n = read16(bmpFile);        // # planes -- must be '1'
    bmpDepth = read16(bmpFile); // bits per pixel
    pos = read32(bmpFile);      // format
    if (bmpID != 0x4D42) ret = 2; // bad ID
    else if (n != 1) ret = 3;   // too many planes
    else if (pos != 0 && pos != 3) ret = 4; // format: 0 = uncompressed, 3 = 565
    else if (bmpDepth < 16 && bmpDepth > PALETTEDEPTH) ret = 5; // palette 
    else {
        bool first = true;
        is565 = (pos == 3);               // ?already in 16-bit format
        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * bmpDepth / 8 + 3) & ~3;
        if (bmpHeight < 0) {              // If negative, image is in top-down order.
            bmpHeight = -bmpHeight;
            flip = false;
        }

        w = bmpWidth;
        h = bmpHeight;
        if ((x + w) >= tft.width())       // Crop area to be loaded
            w = tft.width() - x;
        if ((y + h) >= tft.height())      //
            h = tft.height() - y;

        if (bmpDepth <= PALETTEDEPTH) {   // these modes have separate palette
            //bmpFile.seek(BMPIMAGEOFFSET); //palette is always @ 54
            bmpFile.seek(bmpImageoffset - (4<<bmpDepth)); //54 for regular, diff for colorsimportant
            bitmask = 0xFF;
            if (bmpDepth < 8)
                bitmask >>= bmpDepth;
            bitshift = 8 - bmpDepth;
            n = 1 << bmpDepth;
            lcdbufsiz -= n;
            palette = lcdbuffer + lcdbufsiz;
            for (col = 0; col < n; col++) {
                pos = read32(bmpFile);    //map palette to 5-6-5
                palette[col] = ((pos & 0x0000F8) >> 3) | ((pos & 0x00FC00) >> 5) | ((pos & 0xF80000) >> 8);
            }
        }

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x + w - 1, y + h - 1);
        for (row = 0; row < h; row++) { // For each scanline...
            // Seek to start of scan line.  It might seem labor-
            // intensive to be doing this on every line, but this
            // method covers a lot of gritty details like cropping
            // and scanline padding.  Also, the seek only takes
            // place if the file position actually needs to change
            // (avoids a lot of cluster math in SD library).
            uint8_t r, g, b, *sdptr;
            int lcdidx, lcdleft;
            if (flip)   // Bitmap is stored bottom-to-top order (normal BMP)
                pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
            else        // Bitmap is stored top-to-bottom
                pos = bmpImageoffset + row * rowSize;
            if (bmpFile.position() != pos) { // Need seek?
                bmpFile.seek(pos);
                buffidx = sizeof(sdbuffer); // Force buffer reload
            }

            for (col = 0; col < w; ) {  //pixels in row
                lcdleft = w - col;
                if (lcdleft > lcdbufsiz) lcdleft = lcdbufsiz;
                for (lcdidx = 0; lcdidx < lcdleft; lcdidx++) { // buffer at a time
                    uint16_t color;
                    // Time to read more pixel data?
                    if (buffidx >= sizeof(sdbuffer)) { // Indeed
                        bmpFile.read(sdbuffer, sizeof(sdbuffer));
                        buffidx = 0; // Set index to beginning
                        r = 0;
                    }
                    switch (bmpDepth) {          // Convert pixel from BMP to TFT format
                        case 32:
                        case 24:
                            b = sdbuffer[buffidx++];
                            g = sdbuffer[buffidx++];
                            r = sdbuffer[buffidx++];
                            if (bmpDepth == 32) buffidx++; //ignore ALPHA
                            color = tft.color565(r, g, b);
                            break;
                        case 16:
                            b = sdbuffer[buffidx++];
                            r = sdbuffer[buffidx++];
                            if (is565)
                                color = (r << 8) | (b);
                            else
                                color = (r << 9) | ((b & 0xE0) << 1) | (b & 0x1F);
                            break;
                        case 1:
                        case 4:
                        case 8:
                            if (r == 0)
                                b = sdbuffer[buffidx++], r = 8;
                            color = palette[(b >> bitshift) & bitmask];
                            r -= bmpDepth;
                            b <<= bmpDepth;
                            break;
                    }
                    lcdbuffer[lcdidx] = color;

                }
                tft.pushColors(lcdbuffer, lcdidx, first);
                first = false;
                col += lcdidx;
            }           // end cols
        }               // end rows
        tft.setAddrWindow(0, 0, tft.width() - 1, tft.height() - 1); //restore full screen
        ret = 0;        // good render
    }
    bmpFile.close();
    return (ret);
}

void setup()
{
    tft.reset();
    uint16_t ID;
    Serial.begin(9600);
    Serial.print("Show BMP files on TFT with ID:0x");
    ID = tft.readID();
    Serial.println(ID, HEX);
    if (ID == 0x0D3D3) ID = 0x9481;
    tft.begin(ID);
    tft.fillScreen(0xffff);
    tft.setRotation(3);
    tft.setTextColor(0xFFFF, 0x0000);
    bool good = SD.begin(SD_CS);
    if (!good) {
        Serial.print(F("cannot start SD"));
        while (1);
    }
    root = SD.open(namebuf);
    pathlen = strlen(namebuf);
      
    col[0] = tft.color565(155, 0, 50);
    col[1] = tft.color565(170, 30, 80);
    col[2] = tft.color565(195, 60, 110);
    col[3] = tft.color565(215, 90, 140);
    col[4] = tft.color565(230, 120, 170);
    col[5] = tft.color565(250, 150, 200);
    col[6] = tft.color565(255, 180, 220);
    col[7] = tft.color565(255, 210, 240);

    // input setup

  pinMode(airSensor, INPUT);
  pinMode(soilMoisture, INPUT);
  pinMode(watersensor, INPUT);

  // output setup
  pinMode(relayPin, OUTPUT);
  pinMode(LED, OUTPUT);

  Wire.begin();
  lightMeter.begin();

}
int index = 0;
//#define NAMEMATCH "main"         // *tiger*.bmp
Adafruit_GFX_Button *buttons[] = {&on_btn, &off_btn, NULL};
/* update the state of a button and redraw as reqd
 *
 * main program can use isPressed(), justPressed() etc
 */
bool update_button(Adafruit_GFX_Button *b, bool down)
{
    b->press(down && b->contains(pixel_x, pixel_y));
    if (b->justReleased())
        b->drawButton(false);
    if (b->justPressed())
        b->drawButton(true);
    return down;
}
/* most screens have different sets of buttons
 * life is easier if you process whole list in one go
 */
bool update_button_list(Adafruit_GFX_Button **pb)
{
    bool down = Touch_getXY();
    for (int i = 0 ; pb[i] != NULL; i++) {
        update_button(pb[i], down);
    }
    return down;
}

int control_type = 1;

void loop()
{ 
  tft.fillScreen(0);
  if (index==0){
    char namebuf[32] = "/logo.bmp";
    showBMP(namebuf, 0, 0);
    delay(1000);

  }
  tft.fillScreen(0);
  

  if (index==0){
      showmsgXY(170, 250, 2, &FreeSans9pt7b, "Loading...", 0xFFFF);
      for (int i = 8; i > 0; i--) {
        tft.fillCircle(240 + 40 * (cos(-i * PI / 4)), 120 + 40 * (sin(-i * PI / 4)), 10,  col[0]); delay(15);
        tft.fillCircle(240 + 40 * (cos(-(i + 1)*PI / 4)), 120 + 40 * (sin(-(i + 1)*PI / 4)), 10,  col[1]); delay(15);
        tft.fillCircle(240 + 40 * (cos(-(i + 2)*PI / 4)), 120 + 40 * (sin(-(i + 2)*PI / 4)), 10,  col[2]); delay(15);
        tft.fillCircle(240 + 40 * (cos(-(i + 3)*PI / 4)), 120 + 40 * (sin(-(i + 3)*PI / 4)), 10,  col[3]); delay(15);
        tft.fillCircle(240 + 40 * (cos(-(i + 4)*PI / 4)), 120 + 40 * (sin(-(i + 4)*PI / 4)), 10,  col[4]); delay(15);
        tft.fillCircle(240 + 40 * (cos(-(i + 5)*PI / 4)), 120 + 40 * (sin(-(i + 5)*PI / 4)), 10,  col[5]); delay(15);
        tft.fillCircle(240 + 40 * (cos(-(i + 6)*PI / 4)), 120 + 40 * (sin(-(i + 6)*PI / 4)), 10,  col[6]); delay(15);
        tft.fillCircle(240 + 40 * (cos(-(i + 7)*PI / 4)), 120 + 40 * (sin(-(i + 7)*PI / 4)), 10,  col[7]); delay(15);
    }
    index = 1;
    tft.fillScreen(0);
  }
  if (control_type == 1){
    autocontrol();
  }
  else{
    manualcontrol();
  }
  

}

void autocontrol(){
  char namebuf2[32] = "/main.bmp";
  showBMP(namebuf2, 0, 0);

  on_btn.initButtonUL(&tft,  0, 0, 40, 40, WHITE, CYAN, BLACK, "", 2);
  on_btn.drawButton(false);
  showmsgXY(7, 30, 2, &FreeSans9pt7b, "A",BLACK);
  update_button_list(buttons);  //use helper function
    if (on_btn.justPressed()) {
        tft.fillRect(40, 80, 160, 80, GREEN);
    }


  // DHT11 humidity and temperature data BEGIN-----------------------------------------------
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  // start working...
  Serial.println("=================================");
  // if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
  //   Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
  //   return;
  //   }
  Serial.print("Humidity = ");   
  Serial.print((int)humidity);   
  Serial.print("% , ");   
  Serial.print("Temperature = ");   
  Serial.print((int)temperature);   
  Serial.println("C ");  

  showmsgXY(310, 280, 1, &FreeSans6pt7b, "Humidity:",0xFFFF);
  char humi[4];
  String Humity = String(humidity);
  Humity.toCharArray(humi,4);
  showmsgXY(310, 295, 1, &FreeSans6pt7b, humi,0xFFFF);
  showmsgXY(350, 295, 1, &FreeSans6pt7b, "%",0xFFFF);
  tft.fillCircle(331,220, 28,  0x6D7C);
  humidity = humidity*1.5;
  // DATA FOR TEST : humidity = 50*1.5;
  tft.fillRoundRect(320, 200-(int)humidity, 22, (int)humidity, 10, 0x6D7C);



  showmsgXY(390, 280, 1, &FreeSans6pt7b, "Temperature:" ,0xFFFF);
  char Temp[4];
  String temper = String(temperature);
  temper.toCharArray(Temp,4);
  showmsgXY(390, 295, 1, &FreeSans6pt7b, Temp,0xFFFF);
  showmsgXY(440, 295, 1, &FreeSans6pt7b, "oC",0xFFFF);
  tft.fillCircle(417,220, 28,  0xF1AA);

  // DATA FOR TEST : temperature = 100;

  tft.fillRoundRect(405, 200-(int)temperature, 22, (int)temperature, 10, 0xF1AA);



  // DHT11 humidity and temperature data END-----------------------------------
 
  // MQ-135  Gas Sensor BEGIN--------------------------------------------------
  // float rzero = gasSensor.getRZero(); 
  // this to get the rzero value
  // Serial.print("RZero=");
  // Serial.println(rzero); 
     
  // float ppm = gasSensor.getPPM(); 
  // Serial.print("PPM=");
  // Serial.println(ppm); 
  Serial.println("=================================");  
  float ppmbalanced = gasSensor.getCorrectedPPM(temperature, humidity); 
  Serial.print("PPM Corrected=");
  Serial.println(ppmbalanced);

  char ppmb[4];
  String ppmbA = String(ppmbalanced);
  ppmbA.toCharArray(ppmb,4);
  showmsgXY(80,100, 1, &FreeSans6pt7b, ppmb, 0xFFFF);
  showmsgXY(120,100, 1, &FreeSans6pt7b, "PPM",0xFFFF);

  //PPM range 10-1000
  if (ppmbalanced < 200) {
    Serial.println("Healthy and normal");
    showmsgXY(50,80, 1, &FreeSans6pt7b, "Healthy and normal", 0x0483); //green
  }
  else if (ppmbalanced < 500){
    Serial.println("Acceptable");
    showmsgXY(75,80,  1, &FreeSans6pt7b, "Acceptable",YELLOW);
  }
  else if (ppmbalanced < 700){
    Serial.println("General drowsiness!");
    showmsgXY(50,80,  1, &FreeSans6pt7b, "General drowsiness!",ORANGE);
  }
  else if (ppmbalanced <= 1000){
    Serial.println("Unhealthy & Warrining!!!");
    showmsgXY(50,80, 2, &FreeSans6pt7b, "Unhealthy Warrining!!!",RED);
  }
  // MQ-135  Gas Sensor END-----------------------------------

  // Soil Sensor BEGIN----------------------------------------
  Serial.println("=================================");  
  int soildata = analogRead(soilMoisture);
  soildata = map(soildata, 1023, 0, 0, 100);
  Serial.print("Mositure:");
  Serial.print(soildata);
  Serial.println("%");
  // DATA FOR TEST : soildata=50;
  tft.fillRoundRect(50, 130, 10, (int)soildata, 5, 0x0483);
  if (soildata <= 10){
    Serial.println("pump start!");
    showmsgXY(33,300, 2, &FreeSans6pt7b, "Need Water", BLUE);


    waterPump();
  }
  else{
    Serial.println("pump stop!");
    showmsgXY(35,300, 1, &FreeSans6pt7b, "Healthy", GREEN);

    stopPump();
  }
  // Soil Sensor END----------------------------------------

  // Light Sensor BEGIN-------------------------------------
  Serial.println("================================="); 
  float lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");
  showmsgXY(235, 280, 1, &FreeSans6pt7b, "Light:" ,0xFFFF);
  char Ligh[4];
  String LUX = String(temperature);
  LUX.toCharArray(Ligh,4);
  showmsgXY(235, 295, 1, &FreeSans6pt7b, Ligh,0xFFFF);
  showmsgXY(260, 295, 1, &FreeSans6pt7b, "Lx",0xFFFF);
  // DATA FOR TEST : lux = 700+5000;
  tft.fillRoundRect(245, 250-(lux/284), 22, (lux/284), 2, 0xFFA0);

  // Light Sensor END---------------------------------------

  // WATER LEVEL BEGIN-------------------------------------
  int waterdata = analogRead(watersensor);;
  Serial.println("================================="); 
  Serial.print("waterdata: ");
  Serial.println(waterdata);
  // DATA FOR TEST : waterdata = 700;
  int waterdatadisplay = waterdata/10;
  tft.fillRoundRect(130, 260-(int)waterdatadisplay, 68, (int)waterdatadisplay, 5, 0x7EFE);
  if (waterdata > 650) {
    Serial.println("High Level");
    showmsgXY(120,300, 1, &FreeSans6pt7b, "High Level", 0x0483);
  }          
  else if ((waterdata > 550) && (waterdata <= 650)) {
    Serial.println("Low Level");
    showmsgXY(120,300, 1, &FreeSans6pt7b, "Low Level", YELLOW);
  }     
  else if (waterdata <=550){
    Serial.println("NO Water");
    showmsgXY(120,300, 2, &FreeSans6pt7b, "NO Water", RED);
    //digitalWrite (red,HIGH);      
  }

  delay(30000); //1 min
  control_type = 0; //TEST
  return;
}
//http://adafruit.github.io/Adafruit-GFX-Library/html/class_adafruit___g_f_x___button.html
void manualcontrol(){
  on_btn.initButtonUL(&tft,  0, 0, 40, 40, WHITE, CYAN, BLACK, "", 3);
  on_btn.drawButton(false);
  showmsgXY(5, 30, 2, &FreeSans9pt7b, "M",BLACK);

  update_button_list(buttons);  //use helper function
    if (on_btn.justPressed()) {
        tft.fillRect(40, 80, 160, 80, GREEN);
    }
  delay(30000);
  control_type = 1; //TEST
  return;
}
