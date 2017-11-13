/*
  SOAP to RATP
  Author : E. Grolleau
  Date : 2017/09/27
  V2 

  Do not modify this program,
  Used memory is maximum.
 */

#include <SPI.h>
#include <Ethernet.h> // TO have Ethernet
#include <Wire.h> // To connect other Arduino card

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0x90, 0xA2, 0xDA, 0x10, 0xF4, 0x11 };
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // numeric IP for Google (no DNS)
char RATPserver[] = "opendata-tr.ratp.fr";    // name address 
char weatherServer[] = "api.openweathermap.org";    // name address 
//char weatherServer[] = "www.google.fr";    // name address 

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 17);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

unsigned long lastConnectionTime = 0;             // last time you connected to the server, in milliseconds
// In order to not saturated the RATP server, we wait at least 30s between each server
const unsigned long postingInterval = 25L * 1000L; // delay between updates, in milliseconds (here 30s)
// the "L" is needed to use long type numbers

// strlen("</weatherdata>") + 1
#define LAST_CHARACTER_NUMBER 15
// keep the last LAST_CHARACTER_NUMBER characters
char lastword[LAST_CHARACTER_NUMBER] ={'\0'};

void setup() {

  // Initialize the connection with the LCD board
  Wire.begin(); // join i2c bus (address optional for master) 
     
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Serial port connected");

  EthernetConnection();

  // We only do the weather request at the initialization phase
  // because the weather do not change each time
  weatherHttpRequest();
}

void loop() {
  // if there are incoming bytes available
  // from the server, read them and print them:
  //Serial.println("Loop...");
  
  if (client.available()) 
  {
    // Read Character by character
    char c = client.read();
    Serial.print(c);

    /*
     * Send message to Arduino in charge of LCD display
     */
    Wire.beginTransmission(8); // transmit to device #8  
    Wire.write(c);     
    Wire.endTransmission();    // stop transmitting  

    /*
     * Section of code to save the last LAST_CHARACTER_NUMBER
     * within the variable lastword
     * in order to analyze this variable below
     */
    /* Fill lastword variable
     * for the first of first affectation
     * after that lastword will be a circular buffer 
     * Not enough memory to manage cleanly the init of the last word
     * like in slave code, but we are looking for something at the end of the answer 
     */
    memmove(lastword, lastword+1, LAST_CHARACTER_NUMBER-1);
    lastword[LAST_CHARACTER_NUMBER-2] = c;
    //lastword[LAST_CHARACTER_NUMBER-1] = '\0';
    //Serial.println(lastword);

    // Find the end of weather data answer
    // look for "<soapenv:Envelope " within lastword
    if ( strcmp("</weatherdata>",lastword) == 0)
    {
      // Found end of weather data answer
      // We stop immediatly to clean the socket and
      // launch the RATP request
      Serial.println("\n/weatherdata");
      client.stop();
    }
   
  }
  else
  {
    // if postingInterval seconds have elapsed since your last connection,
    // then connect again and send data:
    if (millis() - lastConnectionTime > postingInterval 
      && !client.connected()) 
    {
      RATPsoapRequest(); 
    }
  }
}

/* 
 * this method makes a HTTP SOAP connection to the server: 
 */
void RATPsoapRequest() {
  // close any connection before send a new request.
  // This will free the socket on the Ethernet shield
  client.stop();

  // if there's a successful connection:
  // if you get a connection, report back via serial:
  if (client.connect(RATPserver, 80)) 
  {
    Serial.println("\nConnected to RATP SOAP");
    
    /* 
     *  Make a SOAP HTTP request:
     */
    //client.println("POST http://opendata-tr.ratp.fr/wsiv/services/Wsiv HTTP/1.1");
    client.println("POST /wsiv/services/Wsiv HTTP/1.1");
    //client.println("Accept-Encoding: gzip,deflate");
    client.println("Content-Type: text/xml;charset=UTF-8");
    // Mandatory SOAPAction
    client.println("SOAPAction: \"urn:getMissionsNext\"");
    // MANDATORY Content-length (au moins >= au vrai nombre, sinon ne fonctionne pas)
    // La valeur ci-dessous tient compte de tous les caracteres à partir de 
    // la premiere ligne apres le content-length et jusqu'au bout, tient compte des espaces
    // mais en général on n'en met pas
    // Il faut tester et mettre le plus petit chiffre possible. Dans ce cas la requete d'une 1 seconde, sinon 1 minute
    // Il faut procéder par itération et prendre la plus petite valeur qui fonctionne
    client.println("Content-Length: 475"); //480 à 490 parfait
    client.println("Host: opendata-tr.ratp.fr");
    // Ne pas mettre, ne fonctionne pas Connection: Keep-Alive
    //client.println("Connection: Keep-Alive");
    client.println("Connection: close");
    //MANDATORY : blank line that indicates "end of headers".
    client.println();
    client.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    client.println("<soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:wsiv=\"http://wsiv.ratp.fr\" xmlns:xsd=\"http://wsiv.ratp.fr/xsd\">");
    client.println("<soapenv:Header/>");
    client.println("<soapenv:Body>");
    client.println("<wsiv:getMissionsNext>");
    client.println("<wsiv:station>");
    client.println("<xsd:line>");
    client.println("<xsd:id>M7</xsd:id>");
    client.println("</xsd:line>");
    client.println("<xsd:name>Villejuif - Louis Aragon</xsd:name>");
    client.println("</wsiv:station>");
    client.println("<wsiv:direction>");
    client.println("<xsd:sens>R</xsd:sens>");
    client.println("</wsiv:direction>");
    client.println("</wsiv:getMissionsNext>");
    client.println("</soapenv:Body>");
    client.println("</soapenv:Envelope>");
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
  } 
  else 
  {
    // if you didn't get a connection to the server:
    Serial.println("\nConnection to SOAP RATP failed");
    // Try to connect again
    EthernetConnection();
  }
}

/* 
 * this method makes an HTTP connection to the weather server: 
 */
void weatherHttpRequest() 
{
  
  // close any connection before send a new request.
  // This will free the socket on the Ethernet shield
  //client.stop();
  Serial.println("Connecting to Weather site");
 
  // if there's a successful connection:
  // if you get a connection, report back via serial:

  if (int res = client.connect(weatherServer, 80)) 
  {
    Serial.println("\nConnected to Weather site");
    
     // Make an HTTP request:    
     client.println("GET /data/2.5/forecast?q=Paris,fr&type=like&mode=xml&units=metric&lang=en&cnt=1&appid=1100111e7538466a3b09bd7dab77a877 HTTP/1.1");
     client.println("Host: api.openweathermap.org");
     client.println("Connection: close");
     client.println();

    // note the time that the connection was made:
    //lastConnectionTime = millis();
  }
  else 
  {
    // if you didn't get a connection to the server:
    Serial.println("Connection to weather forecast website failed");
    
     //* SUCCESS 1    
     //* TIMED_OUT -1    
     //* INVALID_SERVER -2    
     //* TRUNCATED -3    
     //* INVALID_RESPONSE -4
    
    Serial.println(res);
    // Try to connect again
    //EthernetConnection();
  }
}


void EthernetConnection()
{
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) 
  {
    Serial.println("Failed to configure Ethernet DHCP");
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }

  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("connecting...");
}




