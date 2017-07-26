#include <Ethernet.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address
int numberOfDevices; // Number of temperature devices found

#define DEBUG 1
#define FEEDS 3

String feeds[] = {"tank_top_temp", "tank_bottom_temp", "ambient_temp"};

#if (DEBUG==1)
#define PRINT Serial.println
#endif
#if (DEBUG==0)
#define PRINT nullprint
#endif

#define SECOND 1000
#define MINUTE 60000

int DELAY_MINUTES = 1;
unsigned long timeRef;
unsigned long secondRef;

const int lightPin = 0;  // Define a pin for Photo resistor
const int brightnessDifference = 150; // Approximate minimum difference between the washer light when it's on and when it's off
int lastBrightness = analogRead(lightPin);
bool washerOn = false;

const int buttonPin = 3;     // The number of the pushbutton pin
int buttonState = 0;         // Variable for reading the pushbutton status

#define NORMAL_STATE 1
#define ERROR_STATE -1

int state = NORMAL_STATE;
char temps[FEEDS][10];

const int red =  7;
const int yellow =  8;
const int green =  9;

boolean clientConnected = false;
EthernetClient client;


void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(yellow, OUTPUT);

  #if (DEBUG==1)
    Serial.begin(9600);
  #endif
  PRINT("Starting init");

  sensors.begin();
  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();

  initialize();
  delay(SECOND);

  PRINT("Init done");

  // Init timeRef
  timeRef = millis();
  secondRef = millis();
}

// Set up the ethernet connection
void initialize() {
  digitalWrite(yellow, HIGH);
  digitalWrite(green, HIGH);
  digitalWrite(red, HIGH);

  byte mac[] = {  0xB8, 0x27, 0xEB, 0xFE, 0xB2, 0x49 }; // unused mac address
  if (Ethernet.begin(mac) == 0) {
    PRINT("Failed to configure Ethernet using DHCP");
    state = ERROR_STATE;
  }
  else {
      Serial.print("Etherent configured using DHCP.\nIP : ");
      for (byte thisByte = 0; thisByte < 4; thisByte++) {
        // print the value of each byte of the IP address:
        Serial.print(Ethernet.localIP()[thisByte], DEC);
        Serial.print(".");
      }
      PRINT();
      state = NORMAL_STATE;
      digitalWrite(red, LOW);
      digitalWrite(yellow, LOW);
      delay(1000);
  }
}

void loop() {
  if (state == NORMAL_STATE) {
    normal_state();
  }
  else if (state == ERROR_STATE) {
    error_state();
  }
}

int es = 1;
void error_state() {
  int bs = digitalRead(buttonPin);
  if (buttonState == LOW && bs == HIGH) {
    initialize();
  }
  buttonState = bs;
  if (millis() - timeRef > 1000) {
    timeRef = millis();
    es *= -1;
    if (es == 1) {
      display(HIGH,HIGH,HIGH);
    }
    else {
      display(LOW,LOW,LOW);
    }
  }
}

int minute_count = 1;

void normal_state() {
    int count;
    buttonState = digitalRead(buttonPin);

    // Check every second if the washer has turned on
    if (millis() - secondRef > SECOND) {
        int brightness = analogRead(lightPin);
        if (brightness >= lastBrightness + brightnessDifference) {
            washerOn = true;
            PRINT("LIGHT ON");
        } else if (brightness <= lastBrightness - brightnessDifference) {
            washerOn = false;
            PRINT("LIGHT OFF");
        }
        lastBrightness = brightness;
        secondRef = millis();
    }

    if (millis() - timeRef > MINUTE) {
      minute_count++;
      timeRef = millis();
    #if (DEBUG==1)
      Serial.print("minute : ");
      PRINT(minute_count,DEC);
    #endif
    }
    // Send the values from input feeds every 10 minute or so
    if ((minute_count >= DELAY_MINUTES) || (buttonState == HIGH) ) {
      digitalWrite(yellow, HIGH);
      digitalWrite(green, LOW);
      digitalWrite(red, LOW);
      PRINT("Getting Temperatures");
      count = getTemps();
      PRINT("Sending Temperatures");

      if (post(count, temps)) {
        PRINT("Sending Temperatures: success");
        digitalWrite(yellow, LOW);
        digitalWrite(green, HIGH);
      } else {
        PRINT("Sending Temperatures: failure");
        digitalWrite(red, HIGH);
      }
      if (buttonState == HIGH) {
        delay(1000);
        buttonState = LOW;
      }
      else {
        minute_count = 0;
      }
    }
    else {
      delay(200);
    }
}

bool post(int count, char temps[FEEDS][10]) {
    String postData = String("{\"access_key\":\"1bc7bbdc\", \"");

    for (int i = 0; i < FEEDS; ++i) {
        #if (DEBUG==1)
            Serial.print("Feed ");
            Serial.print(feeds[i]);
            Serial.print(" has value: ");
            PRINT(temps[i]);
        #endif

        postData += feeds[i];
        postData += "\":";
        postData += temps[i];
        postData += ", \"";
    }
    postData += (washerOn) ? "washer3_on\":1" : "washer3_on\":0";
    postData += "}";

   if(!postPage(postData)) {
       PRINT("Error posting");
       return false;
   }
   return true;
}

//-----------------------------------------
//    POST data to server host
//    found at https://forum.arduino.cc/index.php?topic=145277.0
//
bool postPage(String thisData) {
   int inChar;
   if (!clientConnected) {  // if not connected then connect:
       connectToServer();
   }

   if (clientConnected) {
       client.println("POST / HTTP/1.1");  // send the header:
       client.println("Host: 192.168.2.133");
       client.println("Connection: close");
       client.println("Content-Type: application/json");
       client.print("Content-Length: ");
       client.println(thisData.length());
       client.println();
       client.print(thisData);    // send the data:
       PRINT("Sending data " + thisData);
   }
   else {
       PRINT("Heck, were're not connected");
       return false;
   }

   int connectLoop = 0;
   while(client.connected()) {
       while(client.available()) {
           inChar = client.read();
           Serial.write(inChar); // serial print reply from host:
           connectLoop = 0;
       }

       delay(1);
       connectLoop++;
       if(connectLoop > 2500) { // wait up to 2500mSec for reply:
           PRINT();
           PRINT("Timeout");
           client.stop();
           clientConnected = false;
       }
   }

   PRINT("disconnecting");
   client.stop();
   clientConnected = false;
   return true;
}


//-----------------------------------------
//    Connect to Server as Client
//
//
void connectToServer() {
    PRINT("Connecting...");
    uint16_t retry = 3;     // retry count, normally 3:
    while (retry) {
        if (client.connect("192.168.2.133", 8000)) {
            Serial.print("Connection to host succeeded in ");
            Serial.print(3-retry);
            PRINT(" attempts.");
            clientConnected = true;
            break;
        }
        else {
            Serial.print("Connect to host failed, retry: ");
            PRINT(retry);
            clientConnected = false;
        }
       delay(10);   // delay before retry:
        --retry;
    }
}


// Gets the temperatures from the sensors and puts them in the temps array
int getTemps() {
  sensors.requestTemperatures(); // Send the command to get temperatures
  int c = 0;
  for(int i=0; i<FEEDS; i++) {
    #if (DEBUG==1)
      Serial.print(feeds[i]);
      Serial.print(" value = ");
    #endif
    float t = getTempByIndex(i);
    if (t < 1) {
      PRINT(" value < 1, not posting ");
    }
    else {
      PRINT(t,DEC);
      dtostrf(t, 4, 1, temps[c]);
      c++;
    }
  }
  return c;
}

float getTempByIndex(int i) {
    float tempC,tempF;
    String msg = String("");
    if(sensors.getAddress(tempDeviceAddress, i)) {
          msg += "Temperature for device: ";
          msg += i;
          msg += ": ";
          float tempC = sensors.getTempC(tempDeviceAddress);
          msg += f2s(tempC);
          msg += "C, ";
          tempF = DallasTemperature::toFahrenheit(tempC);
          msg += f2s(tempF);
          msg += "F\n";
    }
    else {
        tempF = 0;
    }
    #if (DEBUG==1)
    PRINT(msg);
    #endif
    return tempF;
}

String f2s(float f) {
  char buf[10];
  dtostrf(f, 4, 1, buf);
  return String(buf);
}
void nullprint(char *s) {return;}
void display(int g, int y, int r) {
  digitalWrite(green, g);     digitalWrite(yellow, y);  digitalWrite(red, r);
}
