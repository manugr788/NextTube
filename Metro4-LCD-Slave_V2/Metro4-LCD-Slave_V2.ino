#include <Wire.h>
// include the library code:
#include <LiquidCrystal.h>

// strlen("<stationsMessages>") + 1
#define LAST_CHARACTER_NUMBER 19
// station message max length (!! contain </stationsMessages>)
#define STATION_MESSAGE_MAX_LENGTH (LAST_CHARACTER_NUMBER+10)
// Message only contains the message (without </stationsMessages>)
#define MESSAGE_LENGTH 17 // Since we have a 2 * 16 line LCD

#define btnRIGHT    5
#define btnUP       4
#define btnDOWN     3
#define btnLEFT     2
#define btnSELECT   1
#define btnNONE     0

#define BACKLIGHT_HIGH 250
#define BACKLIGHT_LOW 0 
#define BACKLIGHT_PIN 10

// Which line to use
#define TUBE_LCD_LINE 0
#define WEATHER_LCD_LINE 1

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 8, en = 9, d4 = 4, d5 = 5, d6 = 6, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

unsigned long backlightHighLastStartTime = 0L;             // last time the backlight has begun to be high, in seconds
const unsigned long backlightHighMaxDelay = 300L; // LCD Wake up delay before turn the backlight off (in seconds)
int backlighIsHigh = 0; // Init 

// keep the last LAST_CHARACTER_NUMBER characters
char lastword[LAST_CHARACTER_NUMBER] ={'\0'};
int charIndex =  0;
int StationsMessagesSectionCharIndex = 0;
char StationsMessages_1[STATION_MESSAGE_MAX_LENGTH] ={'\0'};
char StationsMessages_2[STATION_MESSAGE_MAX_LENGTH] ={'\0'};
//char StationsMessages_3[STATION_MESSAGE_MAX_LENGTH] ={'\0'};
bool isWithinStationsMessages = 0;
bool numericalMessageEnded = 1;
int stationsMessagesIndex = 0;
bool isWithinWeatherSymbol = 0;
bool isWithinWeatherSymbolName = 0;
char *ptrWeatherMessage = NULL;
char WeatherMessage[MESSAGE_LENGTH] = {'\0'} ;

void setup() 
{
  // Wire is the library for I2C bus communication
  // La connexion est réalisée par l'intermédiaire de deux lignes :
  // SDA (Serial Data Line) : ligne de données bidirectionnelle,
  // SCL (Serial Clock Line) : ligne d'horloge de synchronisation bidirectionnelle.

  Wire.begin(8);                // join i2c bus with address #8
  Wire.onReceive(receiveEvent); // register event
  Serial.begin(9600);           // start serial for output

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Init the backlight at High level
  analogWrite(BACKLIGHT_PIN, BACKLIGHT_HIGH);
  backlighIsHigh = 1;    
  /*
   * Splash Screen
   */
  lcd.setCursor(0, 0);
  lcd.print("RATP->NextTube  ");
}

void loop() 
{
  // To work only every 100 millisecond to save energy
  delay(100);
  /*
   * We use the select button to set the backlight
   */
  int lcd_key = read_LCD_buttons();
  //Serial.print(lcd_key);

  // If the user pish the SLECT button
  // Set backlight High
  if (lcd_key == btnSELECT)
  {
    // Wake up the LCD
    analogWrite(BACKLIGHT_PIN, BACKLIGHT_HIGH);
    backlighIsHigh = 1;
    backlightHighLastStartTime = millis() / 1000;
  }

  // If time the time since the last LCD wake > 
  // set LCD backlight low
  if( backlighIsHigh == 1
      & (millis() / 1000) -   backlightHighLastStartTime > backlightHighMaxDelay )
  {
    Serial.println("LCD high time elapsed");
    // turn off the backlight to save energy
    analogWrite(BACKLIGHT_PIN, BACKLIGHT_LOW);
    backlighIsHigh = 0; // To not set the backlight off each loop
  }
}

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany) {

  while (Wire.available()) 
  { 
     // We have received some datas from arduino Ethernet
    char c = Wire.read(); // receive byte as a character

    /*
     * Analyze the data
     */
    parseData(c);
    
  }
}

int read_LCD_buttons()
{
  int adc_key_in = analogRead(0);

  if (adc_key_in > 1000)  return btnNONE;
  if (adc_key_in < 50)    return btnRIGHT;
  if (adc_key_in < 250)   return btnUP;
  if (adc_key_in < 450)   return btnDOWN;
  if (adc_key_in < 650)   return btnLEFT;
  if (adc_key_in < 850)   return btnSELECT;

  return btnNONE;
}

/*
 * Show tube information on the LCD
 */
void tubeLCDDisplay(char *waitingTime1, char *waitingTime2)
{
  char message[MESSAGE_LENGTH] ={'\0'};


  strcat(message, "M7: ");
  strncat(message, waitingTime1, 2);
  strcat(message, " mn ");
  strncat(message, waitingTime2, 2);
  strcat(message, " mn  ");
  Serial.println(message);

  lcd.setCursor(0, TUBE_LCD_LINE);
  lcd.print(message);
}

/*
 * Show Weather information on the LCD
 */
void weatherLCDDisplay(char *weatherMessage)
{
  lcd.setCursor(0, WEATHER_LCD_LINE);
  lcd.print("                ");
  lcd.setCursor(0, WEATHER_LCD_LINE);
  lcd.print(weatherMessage);
}

void parseData(char c)
{

    /*
     * ============================================
     *  Save the last LAST_CHARACTER_NUMBER characters
     *  within the variable lastword 
     *  in order to analyze this variable below
     * ============================================
     */
    // Fill lastword variable
    // for the first of first affectation
    // after that lastword will be a circular buffer 
    if ( charIndex < LAST_CHARACTER_NUMBER-1)
    {
      // Init lastword
      lastword[charIndex] = c;
      charIndex++;
    } 
    else
    {
        // last word is now a circular buffer
        if(c != '\n')
        {
          // add last character to the end (translation of 1 character)
          memmove(lastword, lastword+1, LAST_CHARACTER_NUMBER-1);
          lastword[LAST_CHARACTER_NUMBER-2] = c;
          lastword[LAST_CHARACTER_NUMBER-1] = '\0';
        }
    }
    
    /*
     * ============================================
     *  Parse RATP SOAP answer
     * ============================================
     */
    /* 
     *  Analyze when we are within the section of the SOAP
     *  response about station message (what we want)
     */
    if(isWithinStationsMessages == 1)
    {
      // We are within stationsMessages section
      // Parse message
      //Serial.print(c);
      // We only want numerical character
      // ASCII 48 = '0', 57 = '9'
      if(c < 48 || c > 57)
      {
        //Serial.print('\t');
        //Serial.println("Not numeric");
        numericalMessageEnded = 1;
      }
      
      if( numericalMessageEnded == 0 )
      {
        // We get a numerical character
        // We have finished to read what we are looking for
        // we save the value within the relevant variable
        switch(stationsMessagesIndex)
        {
          case 1:
            // We are within the first StationsMessagesSection of this SOAP answer
            StationsMessages_1[StationsMessagesSectionCharIndex] = c;
            Serial.println("Station message 1");
            Serial.println(StationsMessages_1);
            break;
          case 2:
            // We are within the second StationsMessagesSection of this SOAP answer
            StationsMessages_2[StationsMessagesSectionCharIndex] = c;
            Serial.println("Station message 2");
            Serial.println(StationsMessages_2);
            break;
          /*case 3:
            // We are within the first StationsMessagesSection of this SOAP answer
            StationsMessages_3[StationsMessagesSectionCharIndex] = c;
            Serial.println("Station message 3");
            Serial.println(StationsMessages_3);
            break;*/
          default:
            break; 
        }
        StationsMessagesSectionCharIndex++;
      }
    }

    /*
     * Analyze the SOAP response to understand where we are 
     * and save the position within relevant variable. 
     * 
     * The result is sent to the LCD display
     */
     
    /*
     * BEGIN ANSWER
     */
    // look for "<soapenv:Envelope " within lastword
    if ( strcmp("<soapenv:Envelope ",lastword) == 0)
    {
      // Found
      Serial.println("FOUND <soapenv:Envelope ");
      // New answer have been received
      Serial.println("New answer received");
      // Initialize the answer parameter
      stationsMessagesIndex = 0;
      memset(StationsMessages_1, 0, STATION_MESSAGE_MAX_LENGTH);
      memset(StationsMessages_2, 0, STATION_MESSAGE_MAX_LENGTH);
      //memset(StationsMessages_3, 0, STATION_MESSAGE_MAX_LENGTH);
    }

    /*
     * END ANSWER
     */
    // look for "</soapenv:Envelope" within lastword
    if ( strcmp("</soapenv:Envelope",lastword) == 0)
    {
      // Found
      Serial.println("FOUND </soapenv:Envelope");
      // End of answer have been received
      Serial.println("End of answer received");
      // Show message
      Serial.println("Station message 1");
      Serial.println(StationsMessages_1);
      Serial.println("Station message 2");
      Serial.println(StationsMessages_2);
      //Serial.println("Station message 3");
      //Serial.println(StationsMessages_3); 

      // We gathered all the required information
      // Here show the result
      tubeLCDDisplay(StationsMessages_1, StationsMessages_2);
      
    }
    /*
     * BEGIN STATION MESSAGE WITHIN ANSWER
     */
    // look for <stationsMessages> within lastword
    if ( strcmp("<stationsMessages>",lastword) == 0)
    {
      // Found
      /* Serial.print("\t");
      Serial.println(lastword);
      Serial.print("\t");
      Serial.println("FOUND <stationsMessages>"); */
      // Initialize sationsMessages parameter
      isWithinStationsMessages = 1;
      // New station messages section
      StationsMessagesSectionCharIndex = 0;
      // Increment the index of the message
      stationsMessagesIndex++;
      // We suppose to start the message with numerical character 
      numericalMessageEnded = 0;
    }

    /*
     * END STATION MESSAGE WITHIN ANSWER
     */
    // look for </stationsMessages within lastword
    if ( strcmp("</stationsMessages",lastword) == 0)
    {
      // Found
      /* Serial.print("\t");
      Serial.println(lastword);
      Serial.print("\t");
      Serial.println("FOUND </stationsMessages>"); */
      isWithinStationsMessages = 0;
    }

    /*
     * ============================================
     *  Parse Weather SOAP answer
     * ============================================
     */
    /* 
     *  Analyze when we are within the section symbol of the 
     *  response (what we want)
     */
    if(isWithinWeatherSymbol == 1)
    {
      // This code section must be before the detection
      // of "name=" because we don't want to pass through
      // the first time time we detect "name="
      if( isWithinWeatherSymbolName == 1)
      {
          // We look for the end \" of the message
          if ( c == '"' ||  strlen(WeatherMessage)+1 >= MESSAGE_LENGTH)
          {
              // End of the message detected
              Serial.println(WeatherMessage);
              weatherLCDDisplay(WeatherMessage);
              isWithinWeatherSymbolName = 0;
          }
          else
          {
              WeatherMessage[strlen(WeatherMessage)] = c;
          }
      }

      // We are within Symbol section
      // Parse message
      // look for name=" within lastword
      if ( strncmp("name=\"",lastword,6) == 0)
      {
        Serial.println(lastword); 
        // We have reach the last character
        // Move to the begining of the message 
        ptrWeatherMessage = strchr(lastword,'"');
        ptrWeatherMessage++; // Jump the first \"
        //memset(WeatherMessage,'\0',MESSAGE_LENGTH);
        strcpy(WeatherMessage, ptrWeatherMessage);
        Serial.println(WeatherMessage); 
        

        //if we have already the last \"
        if ( (ptrWeatherMessage = strchr(WeatherMessage,'"')) != NULL)
        {
            *ptrWeatherMessage = '\0';
            weatherLCDDisplay(WeatherMessage);
        }
        else
        {
             isWithinWeatherSymbolName = 1;
        }
        Serial.println(WeatherMessage); 
      }
    }
    
    /*
     * BEGIN Weather ANSWER, section Symbol
     */
    // look for <symbol> within lastword
    if ( strncmp("<symbol",lastword,7) == 0)
    {
      // Found
      /* Serial.print("\t");
      Serial.println(lastword);*/
      Serial.println("FOUND <symbol"); 
      // Initialize sationsMessages parameter
      isWithinWeatherSymbol = 1;
    }

    /*
     * END Weather ANSWER, section Symbol
     */
    // look for </symbol> within lastword
    if ( strncmp("</symbol>",lastword,9) == 0)
    {
      // Found
      /* Serial.print("\t");
      Serial.println(lastword);*/
      Serial.println("FOUND </symbol>"); 
      // Initialize sationsMessages parameter
      isWithinWeatherSymbol = 0;
    }
}

