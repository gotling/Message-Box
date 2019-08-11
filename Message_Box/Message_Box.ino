// Use the MD_MAX72XX library to scroll text on the display
// received through the ESP8266 WiFi interface.
//
// Demonstrates the use of the callback function to control what
// is scrolled on the display text. User can enter text through
// a web browser and this will display as a scrolling message on
// the display.
//
// IP address for the ESP8266 is displayed on the scrolling display
// after startup initialization and connected to the WiFi network.
//
// Connections for ESP8266 hardware SPI are:
// Vcc       3v3     LED matrices seem to work at 3.3V
// GND       GND     GND
// DIN        D7     HSPID or HMOSI
// CS or LD   D8     HSPICS or HCS
// CLK        D5     CLK or HCLK
//

#include <ESP8266WiFi.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define PRINT_CALLBACK  0
#define DEBUG 1

// Various delays in milliseconds
#define UNIT_DELAY      25
#define MIDLINE_DELAY   (6 * UNIT_DELAY)
#define SCANNER_DELAY   (2 * UNIT_DELAY)
#define RANDOM_DELAY    (6 * UNIT_DELAY)

uint32_t prevTimeAnim = 0;    // Used for remembering the millis() value in animations

#if DEBUG
#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }
#define PRINTS(s)   { Serial.print(F(s)); }
#else
#define PRINT(s, v)
#define PRINTS(s)
#endif

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

#define CLK_PIN   D5 // or SCK
#define DATA_PIN  D7 // or MOSI
#define CS_PIN    D8 // or SS

#define MELODY_PIN  D2

// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Arbitrary pins
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// WiFi login parameters - network name and password
const char* ssid = "";
const char* password = "";

// WiFi Server object and parameters
WiFiServer server(80);

// Global message buffers shared by Wifi and Scrolling functions
const uint8_t MESG_SIZE = 255;
const uint8_t CHAR_SPACING = 1;
const uint8_t SCROLL_DELAY = 75;

enum { S_BOOT, S_WAIT, S_NEW, S_SHOW } global_state = S_BOOT;

char curMessage[MESG_SIZE];
char newMessage[MESG_SIZE];
bool newMessageAvailable = false;

char WebResponse[] = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";

char WebPage[] =
"<!DOCTYPE html>" \
"<html>" \
"<head>" \
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>" \
"<title>is Food</title> " \
"<style>" \
"body, input {font-size: 20pt; margin: 1em; padding: 1em;}" \
"</style>" \
"</head> " \
"" \
"<body> " \
"<h1>is Food</h1>" \
"<br>" \
"<input type=submit value='NOW' onclick=SendText(0)> " \
"<input type=submit value='SOON' onclick=SendText(1)> " \
"<input type=submit value='30 min' onclick=SendText(2)>" \
"<input type=submit value='Food cold :(' onclick=SendText(3)>" \
"<input type=submit value='Kolla telefon' onclick=SendText(4)>" \
"" \
"<script>" \
"        strLine = '';" \
"        " \
"        function SendText(id) " \
"        { " \
"          nocache = '/&nocache=' + Math.random() * 1000000; " \
"          var request = new XMLHttpRequest(); " \
"          strLine = '&ID=' + id;" \
"          request.open('GET', strLine + nocache, false); " \
"          request.send(null); " \
"          document.getElementById('txt_form').Message.value='';" \
"        }" \
"        </script> " \
"</body> " \
"</html>";

char *err2Str(wl_status_t code)
{
  switch (code)
  {
  case WL_IDLE_STATUS:    return("IDLE");           break; // WiFi is in process of changing between statuses
  case WL_NO_SSID_AVAIL:  return("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
  case WL_CONNECTED:      return("CONNECTED");      break; // successful connection is established
  case WL_CONNECT_FAILED: return("CONNECT_FAILED"); break; // password is incorrect
  case WL_DISCONNECTED:   return("CONNECT_FAILED"); break; // module is not configured in station mode
  default: return("??");
  }
}

uint8_t htoi(char c)
{
  PRINT("\nhtoi: ", c);
  c = toupper(c);
  if ((c >= '0') && (c <= '9')) return(c - '0');
  if ((c >= 'A') && (c <= 'F')) return(c - 'A' + 0xa);
  return(0);
}

boolean getText(char *szMesg, char *psz, uint8_t len)
{
  boolean isValid = false;  // text received flag
  char *pStart, *pEnd;      // pointer to start and end of text

  // get pointer to the beginning of the text
  pStart = strstr(szMesg, "/&MSG=");

  if (pStart != NULL)
  {
    pStart += 6;  // skip to start of data
    pEnd = strstr(pStart, "/&");

    if (pEnd != NULL)
    {
      while (pStart != pEnd)
      {
        if (*pStart == '%') {
          char c = 0;
          char c0 = 0;
          char c1 = 0;
          pStart++;
          c0 = *pStart++;
          c1 = *pStart++;
          //PRINT("\ngetText % Not Digit 0: ", c0);
          //PRINT("\ngetText % Not Digit 1: ", c1);

          // %
          if ((c0 == 'C') && (c1 == '3')) {
            continue;
          }

          // å
          if ((c0 == 'A') && (c1 == '5')) {
            c = 229;
          }

          // ä
          else if ((c0 == 'A') && (c1 == '4')) {
            c = 228;
          }

          // ö
          else if ((c0 == 'B') && (c1 == '6')) {
            c = 246;
          }

          // Å
          else if ((c0 == '8') && (c1 == '5')) {
            c = 197;
          }

          // Ä
          else if ((c0 == '8') && (c1 == '4')) {
            c = 196;
          }

          // Ö
          else if ((c0 == '9') && (c1 == '6')) {
            c = 214;
          }

          else {
            c += (htoi(c0) << 4);
            c += htoi(c1);
            PRINT("\ngetText % is Digit: ", c);
          }
         
          
          *psz++ = c;
        } else {
          PRINT("\ngetTextDefault: ", *pStart);
          *psz++ = *pStart++;
        }
      }

      *psz = '\0'; // terminate the string
      isValid = true;
    }
  } else {
    // get pointer to the beginning of the text
    pStart = strstr(szMesg, "/&ID=");
  
    if (pStart != NULL)
    {
      // Play melody on new message
      playMelody();
    
      pStart += 5;  // skip to start of data
      
      if (*pStart == '0') {
        sprintf(psz, "FOOD NOW!!!");
      } else if (*pStart == '1') {
        sprintf(psz, "ALMOST READY!");
      } else if (*pStart == '2') {
        sprintf(psz, "FOOD IN HALF AN HOUR");
      } else if (*pStart == '3') {
        sprintf(psz, "FOOD IS COLD!!!  :'(");
      } else if (*pStart == '4') {
        sprintf(psz, "Kolla din telefon");
      }

      isValid = true;
    }
  }

  return(isValid);
}

void handleWiFi(void)
{
  static enum { S_IDLE, S_WAIT_CONN, S_READ, S_EXTRACT, S_RESPONSE, S_DISCONN } state = S_IDLE;
  static char szBuf[1024];
  static uint16_t idxBuf = 0;
  static WiFiClient client;
  static uint32_t timeStart;

  switch (state)
  {
  case S_IDLE:   // initialize
    PRINTS("\nS_IDLE");
    idxBuf = 0;
    state = S_WAIT_CONN;
    break;

  case S_WAIT_CONN:   // waiting for connection
    {
      client = server.available();
      if (!client) break;
      if (!client.connected()) break;

#if DEBUG
      char szTxt[20];
      sprintf(szTxt, "%03d:%03d:%03d:%03d", client.remoteIP()[0], client.remoteIP()[1], client.remoteIP()[2], client.remoteIP()[3]);
      PRINT("\nNew client @ ", szTxt);
#endif

      timeStart = millis();
      state = S_READ;
    }
    break;

  case S_READ: // get the first line of data
    //PRINTS("\nS_READ");
    while (client.available())
    {
      char c = client.read();
      if ((c == '\r') || (c == '\n'))
      {
        szBuf[idxBuf] = '\0';
        client.flush();
        PRINT("\nRecv: ", szBuf);
        state = S_EXTRACT;
      }
      else
        szBuf[idxBuf++] = (char)c;
    }
    if (millis() - timeStart > 1000)
    {
      PRINTS("\nWait timeout");
      state = S_DISCONN;
    }
    break;


  case S_EXTRACT: // extract data
    PRINTS("\nS_EXTRACT");
    // Extract the string from the message if there is one
    newMessageAvailable = getText(szBuf, newMessage, MESG_SIZE);
    PRINT("\nNew Msg: ", newMessage);
    state = S_RESPONSE;
    break;

  case S_RESPONSE: // send the response to the client
    PRINTS("\nS_RESPONSE");
    // Return the response to the client (web page)
    client.print(WebResponse);
    client.print(WebPage);
    state = S_DISCONN;
    break;

  case S_DISCONN: // disconnect client
    PRINTS("\nS_DISCONN");
    client.flush();
    client.stop();
    state = S_IDLE;
    break;

  default:  state = S_IDLE;
  }
}

void scrollDataSink(uint8_t dev, MD_MAX72XX::transformType_t t, uint8_t col)
// Callback function for data that is being scrolled off the display
{
#if PRINT_CALLBACK
  Serial.print("\n cb ");
  Serial.print(dev);
  Serial.print(' ');
  Serial.print(t);
  Serial.print(' ');
  Serial.println(col);
#endif
}

int repeat = 0;

uint8_t scrollDataSource(uint8_t dev, MD_MAX72XX::transformType_t t)
// Callback function for data that is required for scrolling into the display
{
  static enum { S_IDLE, S_PLAY_MELODY, S_NEXT_CHAR, S_SHOW_CHAR, S_SHOW_SPACE } state = S_IDLE;
  static char *p;
  static uint16_t curLen, showLen;
  static uint8_t  cBuf[8];
  uint8_t colData = 0;

  // finite state machine to control what we do on the callback
  switch (state)
  {
  case S_IDLE: // reset the message pointer and check for new message to load
    //PRINTS("\nS_IDLE");
    p = curMessage;      // reset the pointer to start of message

    if (newMessageAvailable)  // there is a new message waiting
    {
      strcpy(curMessage, newMessage); // copy it in
      newMessageAvailable = false;
      repeat = 2;
      state = S_NEXT_CHAR;
    } else if (repeat > 1) {
      repeat--;
      state = S_NEXT_CHAR;
    }

    break;

  case S_PLAY_MELODY: // Play a melody
    playMelody();

    state = S_NEXT_CHAR;

    break;

  case S_NEXT_CHAR: // Load the next character from the font table
    //PRINTS("\nS_NEXT_CHAR");
    if (*p == '\0')
      state = S_IDLE;
    else
    {
      showLen = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
      curLen = 0;
      state = S_SHOW_CHAR;
    }
    break;

  case S_SHOW_CHAR: // display the next part of the character
    //PRINTS("\nS_SHOW_CHAR");
    colData = cBuf[curLen++];
    if (curLen < showLen)
      break;

    // set up the inter character spacing
    showLen = (*p != '\0' ? CHAR_SPACING : (MAX_DEVICES*COL_SIZE)/2);
    curLen = 0;
    state = S_SHOW_SPACE;
    // fall through

  case S_SHOW_SPACE:  // display inter-character spacing (blank column)
    //PRINT("\nS_ICSPACE: ", curLen);
    //PRINT("/", showLen);
    curLen++;
    if (curLen == showLen)
      state = S_NEXT_CHAR;
    break;

  default:
    state = S_IDLE;
  }

  return(colData);
}

void scrollText(void)
{
  static uint32_t	prevTime = 0;

  // Is it time to scroll the text?
  if (millis() - prevTime >= SCROLL_DELAY)
  {
    mx.transform(MD_MAX72XX::TSL);  // scroll along - the callback will load all the data
    prevTime = millis();      // starting point for next time
  }
}

void resetMatrix(void)
{
  mx.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY/2);
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  mx.clear();
  prevTimeAnim = 0;
}

bool graphicMidline1(bool bInit)
{
  // are we initializing?
  if (bInit)
  {
    PRINTS("\n--- Midline1 init");
    resetMatrix();
    bInit = false;
  }
  else
  {
    for (uint8_t j=0; j<MAX_DEVICES; j++)
    {
      mx.setRow(j, 3, 0xff);
      mx.setRow(j, 4, 0xff);
    }
  }

  return(bInit);
}

bool graphicMidline2(bool bInit)
{
  graphicMidline1(bInit);
 
  static uint8_t  idx = 0;      // position
  static int8_t   idOffs = 1;   // increment direction

  // are we initializing?
  if (bInit)
  {
    PRINTS("\n--- Midline2 init");
    resetMatrix();
    idx = 0;
    idOffs = 1;
    bInit = false;
  }

  // Is it time to animate?
  if (millis()-prevTimeAnim < MIDLINE_DELAY)
    return(bInit);
  prevTimeAnim = millis();    // starting point for next time

  PRINT("\nML2 R:", idx);
  PRINT(" D:", idOffs);

  // now run the animation
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  // turn off the old lines
  for (uint8_t j=0; j<MAX_DEVICES; j++)
  {
    mx.setRow(j, idx, 0x00);
    mx.setRow(j, ROW_SIZE-1-idx, 0x00);
  }

  idx += idOffs;
  if ((idx == 0) || (idx == ROW_SIZE-1))
    idOffs = -idOffs;

  // turn on the new lines
  for (uint8_t j=0; j<MAX_DEVICES; j++)
  {
    mx.setRow(j, idx, 0xff);
    mx.setRow(j, ROW_SIZE-1-idx, 0xff);
  }

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

  return(bInit);
}

bool graphicRandom(bool bInit)
{
  // are we initializing?
  if (bInit)
  {
    PRINTS("\n--- Random init");
    resetMatrix();
    bInit = false;
  }

  // Is it time to animate?
  if (millis()-prevTimeAnim < RANDOM_DELAY)
    return(bInit);
  prevTimeAnim = millis();    // starting point for next time

  // now run the animation
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t i=0; i<mx.getColumnCount(); i++)
    mx.setColumn(i, (uint8_t)random(255));
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

  return(bInit);
}

void setup()
{
#if DEBUG
  Serial.begin(115200);
  PRINTS("\n[MD_MAX72XX WiFi Message Display]\nType a message for the scrolling display from your internet browser");
#endif

  // Display initialization
  mx.begin();
  mx.setShiftDataInCallback(scrollDataSource);
  mx.setShiftDataOutCallback(scrollDataSink);

  curMessage[0] = newMessage[0] = '\0';

  // Connect to and initialize WiFi network
  PRINT("\nConnecting to ", ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    PRINT("\n", err2Str(WiFi.status()));
    sprintf(curMessage, err2Str(WiFi.status()));
    delay(500);
  }
  PRINTS("\nWiFi connected");

  // Start the server
  server.begin();
  PRINTS("\nServer started");

  // Set up first message as the IP address
  sprintf(curMessage, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  //newMessageAvailable = true;
  PRINT("\nAssigned IP ", curMessage);
}

#include "pitches.h"

//Underworld melody
int melody[] = {
  NOTE_C4, NOTE_C5, NOTE_A3, NOTE_A4,
  NOTE_AS3, NOTE_AS4, 0,
  0,
  NOTE_C4, NOTE_C5, NOTE_A3, NOTE_A4,
  NOTE_AS3, NOTE_AS4, 0,
  0
};

//Underwolrd tempo
int tempo[] = {
  12, 12, 12, 12,
  12, 12, 6,
  3,
  12, 12, 12, 12,
  12, 12, 6,
  3
};

void playMelody() {
  int size = sizeof(melody) / sizeof(int);

  delay(1000);
  
  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < size; thisNote++) {

    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / tempo[thisNote];
    tone(MELODY_PIN, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(MELODY_PIN);
  }
}

void loop()
{
  handleWiFi();
  scrollText();
}
