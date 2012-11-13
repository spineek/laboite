/*

 laboite v2.1
 
 Key Features:
 * Indoor Temperature
 * 2 days forecasts
 * Automatic screen brightness adjusting
 * Automatic time (NTP)
 * Weather forecast icons (sunny, cloudy, rain, snow, fog)
 * Next bus arrival information from Keolis real-time API http://data.keolis-rennes.com/
 * Number of available bikes LE vélo STAR
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * Sure Electronics 3216 LED matrix attached to pins 4, 5, 6, 7
 * TinkerKit LDR, Thermistor and Button modules on I0, I1, I2
 
 created 15 Dec 2011
 by Baptiste Gaultier and Tanguy Ropitault
 modified 23 Oct 2012
 by Baptiste Gaultier
 
 */
 
#include <SPI.h>
#include <Ethernet.h>
#include <ht1632c.h>
#include <TinkerKit.h>

#define DEBUG

// initialize the dotmatrix with the numbers of the interface pins (data→7, wr →6, clk→4, cs→5)
ht1632c dotmatrix = ht1632c(&PORTD, 7, 6, 4, 5, GEOM_32x16, 2);

TKLightSensor ldr(I0);    // ldr used to adjust dotmatrix brightness
TKThermistor therm(I1);   // thermistor used for indoor temperature
TKButton button(I2);      // button used to start/stop scrolling

boolean scrolling = true; // value modified when button is pressed

int brightnessValue = 0; // value read from the LDR
int previousBrightnessValue = 512; // previous value of brightness

byte pwm = 8;            // value output to the PWM (analog out)

char hour[3];
char minutes[3];
byte todayIcon;
byte tomorrowIcon;
byte color;
char indoorTemperatureString[3];
byte indoorTemperature;

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x65, 0xA4 };

// fill in an available IP address on your network here,
// for auto-configuration:
IPAddress ip(169, 254, 0, 64);
IPAddress subnet(255, 255, 0, 0);


// initialize the library instance:
EthernetClient client;

const int requestInterval = 10000;     // delay between requests

char serverName[] = "api.baptistegaultier.fr";  // Your favorite API server

boolean requested;                   // whether you've made a request since connecting
long lastAttemptTime = 0;            // last time you connected to the server, in milliseconds

// Variables used to parse the XML from emoncms

String currentLine = "";             // string to hold the text from server
String content = "";
char temperature[3];
char low[3];
char high[3];
char nextBus[3];
char bikesAvailable[3];


boolean readingTime = false;
boolean readingBus = false;
boolean readingBikes = false;
boolean readingTodayIcon = false;
boolean readingTemperature = false;
boolean readingTomorrowIcon = false;
boolean readingLow = false;
boolean readingHigh = false;


// weather forecast sprites:
uint16_t sprites[5][9] =
{
  { 0x0100, 0x0100, 0x2008, 0x1390, 0x0440, 0x0820, 0x682c, 0x0820, 0x0440 },
  { 0x0000, 0x01c0, 0x0230, 0x1c08, 0x2208, 0x4004, 0x4004, 0x3ff8, 0x0000 },
  { 0x01c0, 0x0230, 0x1c08, 0x2208, 0x4004, 0x4004, 0x3ff8, 0x1500, 0x1500 },
  { 0x0000, 0x0000, 0x7ffe, 0x0000, 0x7ffe, 0x0000, 0x7ffe, 0x0000, 0x0000 },
  { 0x0540, 0x0380, 0x1110, 0x0920, 0x1ff0, 0x0920, 0x1110, 0x0380, 0x0540 }
};

// bus sprite
uint16_t busSprite[9] = { 0x00fc, 0x0186, 0x01fe, 0x0102, 0x0102, 0x01fe, 0x017a, 0x01fe, 0x0084};
// bike sprite
uint16_t bikeSprite[9] = { 0x020c, 0x0102, 0x008c, 0x00f8, 0x078e, 0x0ab9, 0x0bd5, 0x0891, 0x070e};

void setup() {
  // reserve space for the strings:
  currentLine.reserve(32);
  content.reserve(8);
  
  // Dotmatrix brightness
  dotmatrix.pwm(pwm);
  
  // initialize serial:
  #ifdef DEBUG
  Serial.begin(9600);
  #endif
  // initialize dotmatrix:
  dotmatrix.clear();
  
  // display a welcome message:
  #ifdef DEBUG
  Serial.println("laboite v2.1 starting...");
  #endif
  
  // attempt a DHCP connection:
  if (!Ethernet.begin(mac)) {
    // if DHCP fails, start with a hard-coded address:
    Ethernet.begin(mac, ip, subnet);
  }
  
  // print your local IP address:
  #ifdef DEBUG
  /*Serial.print("My address: ");
  Serial.println(Ethernet.localIP());*/
  #endif
  
  // connect to API server:
  connectToServer();
}



void loop() {
  if (client.connected()) {
    if (client.available()) {
      // read incoming bytes:
      char inChar = client.read();
      
      #ifdef DEBUG
      Serial.print(inChar);
      #endif

      // add incoming byte to end of line:
      currentLine += inChar; 

      // if you get a newline, clear the line:
      if (inChar == '\n') {
        currentLine = "";
      }
      
      if ( currentLine.endsWith("<time>")) {
        readingTime = true; 
        content = "";
      }

      if (readingTime) {
        if (inChar != '<') {
          if (inChar != '>')
            content += inChar;
        } 
        else {
          readingTime = false;
          hour[0] = content.charAt(0);
          hour[1] = content.charAt(1);
          hour[3] = '\0';
          
          minutes[0] = content.charAt(3);
          minutes[1] = content.charAt(4);
          minutes[3] = '\0';
        }
      }
      
      if (currentLine.endsWith("<nextbus>")) {
        readingBus = true; 
        content = "";
      }

      if (readingBus) {
        if (inChar != '<') {
          if (inChar != '>')
            content += inChar;
        }
        else {
          readingBus = false;
          nextBus[0] = content.charAt(0);
          nextBus[1] = content.charAt(1);
          nextBus[2] = '\0';
        }
      }
      
      if (currentLine.endsWith("<bikesavailable>")) {
        readingBikes = true; 
        content = "";
      }

      if (readingBikes) {
        if (inChar != '<') {
          if (inChar != '>')
            content += inChar;
        }
        else {
          readingBikes = false;
          bikesAvailable[0] = content.charAt(0);
          bikesAvailable[1] = content.charAt(1);
          bikesAvailable[2] = '\0';
        }
      }
      
      if ( currentLine.endsWith("<today>")) {
        readingTodayIcon = true; 
        content = "";
      }

      if (readingTodayIcon) {
        if (inChar != '<') {
          if (inChar != '>')
            content += inChar;
        }
        else {
          readingTodayIcon = false;
          todayIcon = stringToInt(content);
        }
      }
      
      if ( currentLine.endsWith("<temperature>")) {
        readingTemperature = true; 
        content = "";
      }

      if (readingTemperature) {
        if (inChar != '<') {
          if (inChar != '>')
            content += inChar;
        } 
        else {
          readingTemperature = false;
          temperature[0] = content.charAt(0);
          temperature[1] = content.charAt(1);
          temperature[2] = '\0';
        }
      }
      
      if (currentLine.endsWith("<tomorrow>")) {
        readingTomorrowIcon = true; 
        content = "";
      }

      if (readingTomorrowIcon) {
        if (inChar != '<') {
          if (inChar != '>')
            content += inChar;
        }
        else {
          readingTomorrowIcon = false;
          tomorrowIcon = stringToInt(content);
        }
      }
      
      if (currentLine.endsWith("<low>")) {
        readingLow = true; 
        content = "";
      }

      if (readingLow) {
        if (inChar != '<') {
          if (inChar != '>')
            content += inChar;
        }
        else {
          readingLow = false;
          low[0] = content.charAt(0);
          low[1] = content.charAt(1);
          low[2] = '\0';
        }
      }
      
      if (currentLine.endsWith("<high>")) {
        readingHigh = true; 
        content = "";
      }

      if (readingHigh) {
        if (inChar != '<') {
          if (inChar != '>')
            content += inChar;
        } 
        else {
          readingHigh = false;
          high[0] = content.charAt(0);
          high[1] = content.charAt(1);
          high[2] = '\0';
          
          // if you got a ">" character, you've
          // reached the end of the XML          
          dotmatrix.setfont(FONT_5x7);
          
          dotmatrix.putchar(5, 0, hour[0], GREEN);
          dotmatrix.putchar(10, 0, hour[1], GREEN);
          dotmatrix.putchar(14, 0, ':', GREEN);
          dotmatrix.putchar(18, 0, minutes[0], GREEN);
          dotmatrix.putchar(23, 0, minutes[1], GREEN);
          
          dotmatrix.sendframe();
          
          if(scrolling) {
            // Reading the temperature in Celsius degrees and store in the indoorTemperature variable
            indoorTemperature = therm.getCelsius();
            itoa(indoorTemperature, indoorTemperatureString, 10);
            
            for (int x = 32; x > -64; x--) {
              dotmatrix.putchar(x+12, 9, ' ', RED);
              
              if(todayIcon == 0)
                color = ORANGE;
              else
                color = RED;
              dotmatrix.putbitmap(x, 7, sprites[todayIcon],16,9, color);
              
              dotmatrix.putchar(x+12+32, 9, ' ', RED);
              
              if(tomorrowIcon == 0)
                color = ORANGE;
              else
                color = RED;
              dotmatrix.putbitmap(x+32, 7, sprites[tomorrowIcon],16,9, color);
              
              dotmatrix.sendframe();
              if(x >= 0) {
                printTemperature(x+17, temperature[0], temperature[1], RED);
                dotmatrix.sendframe();
              }
              
              if(x >= -32 && x < 0) {
                printTemperature(x+17, indoorTemperatureString[0], indoorTemperatureString[1], ORANGE);
                //printTemperature(x+17, temperature[0], temperature[1], RED);
                printTemperature(x+49, low[0], low[1], RED);
                dotmatrix.sendframe();
              }
              
              if(x >= -63 && x < -32) {
                printTemperature(x+49, high[0], high[1], GREEN);
                dotmatrix.sendframe();
              }
              
              delay(50);
              
              if(x == 0) {
                waitAWhile();
                //delay(800);
                printTemperature(x+17, indoorTemperatureString[0], indoorTemperatureString[1], ORANGE);
                dotmatrix.sendframe();
                waitAWhile();
                //delay(800);
              }
              
              if(x == -32) {
                waitAWhile();
                //delay(800);
                printTemperature(x+49, high[0], high[1], GREEN);
                dotmatrix.sendframe();
                waitAWhile();
                //delay(800);
              }
            }
            
            // next bus
            for (int x = 32; x > -24; x--) {
              adjustBrightness();
              dotmatrix.putchar(x+11, 10, ' ', GREEN);
              
              //dotmatrix.putbitmap(x+1, 7, busSprite, 9, 9, ORANGE);
              dotmatrix.putbitmap(x-1, 7, bikeSprite, 12, 9, ORANGE);
              if(nextBus[0] == '-')
                dotmatrix.putchar(x+11, 9, '<', GREEN);
              else
                dotmatrix.putchar(x+11, 9, nextBus[0], GREEN);
              
              if(nextBus[1] == '\0')
                dotmatrix.putchar(x+5+11, 9, '\'', GREEN);
              else {
                dotmatrix.putchar(x+5+11, 9, nextBus[1], GREEN);
                dotmatrix.putchar(x+10+11, 9, '\'', GREEN);
              }
              
              dotmatrix.sendframe();
              
              delay(50);
              
              if(x == 6 || x == -23)
                waitAWhile();
            }
          }
          
          // close the connection to the server:
          client.stop(); 
        }
      }
    }   
  }
  else if (millis() - lastAttemptTime > requestInterval) {
    // if you're not connected, and ten seconds have passed since
    // your last connection, then attempt to connect again:
    if(button.get())
      scrolling = !scrolling;
    connectToServer();
  }
}

void connectToServer()
{
  // attempt to connect:
  #ifdef DEBUG
  Serial.print("Connecting to api.baptistegaultier.fr...");
  #endif
  if (client.connect(serverName, 80))
  {
    #ifdef DEBUG
    Serial.println("Making HTTP request...");
    #endif
    
    // make HTTP GET request to API server:
    client.println("GET /laboite.xml HTTP/1.1");
    client.println("Host: api.baptistegaultier.fr");
    client.println("User-Agent: Arduino/1.0");
    client.println();
  }
  // note the time of this connect attempt:
  lastAttemptTime = millis();
}

void printTemperature(int x, char firstDigit, char secondDigit, byte color)
{
  if(secondDigit == '\0') {
    secondDigit = firstDigit;
    firstDigit = ' ';
  }
  dotmatrix.putchar(x, 9, firstDigit, color);
  dotmatrix.putchar(x+5, 9, secondDigit, color);
  dotmatrix.putchar(x+10, 9, '*', color);
  
  adjustBrightness();
  
  // print the results to the serial monitor:
  #ifdef DEBUG
  Serial.print("brightness : " );                      
  Serial.println(brightnessValue);
  #endif
}

void adjustBrightness() {
  // read the analog in value:
  brightnessValue = (ldr.get() + previousBrightnessValue) / 2;
  pwm = map(brightnessValue, 0, 1023, 0, 15);
  dotmatrix.pwm(pwm);
  previousBrightnessValue = brightnessValue;
}

void waitAWhile() {
  for (int i = 0; i < 16; i++) {
    adjustBrightness();
    delay(50);
  }
}

int stringToInt(String string) {
  char buffer[8];
  string.toCharArray(buffer, string.length()+1);
  return atoi(buffer);
}
