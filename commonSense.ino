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

const int DELAY_MINUTES = 1;
unsigned long timeRef;
unsigned long secondRef;

const int lightPin = 0;  // Define a pin for Photo resistor
const int brightnessDifference = 150; // Approximate minimum difference between the washer light when it's on and when it's off
int lastBrightness;
bool washer3On;

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

  initializeEthernetConnection();
  delay(SECOND);

  lastBrightness = analogRead(lightPin);
  washer3On = (lastBrightness > brightnessDifference);

  // Init timeRef
  timeRef = -99999;
  secondRef = millis();

  PRINT("Init done");
}

// Set up the ethernet connection
void initializeEthernetConnection() {
  digitalWrite(yellow, HIGH);
  digitalWrite(green, HIGH);
  digitalWrite(red, HIGH);

  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };  // unused mac address
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
    initializeEthernetConnection();
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

void normal_state() {
    buttonState = digitalRead(buttonPin);

    // Check every half second if the washer has turned on or off
    if (millis() - secondRef > (SECOND / 2)) {
        secondRef = millis();
        int brightness = analogRead(lightPin);
        if (brightnessDifference < abs(lastBrightness - brightness)) {
            washer3On = !washer3On;
            String postData = String("{\"w3val\":");
            postData += brightness;
            postData += ", ";
            postData += washerDataToJSON() + "}";
            PRINT("Washer status changed, sending data.");
            post(postData);
        }
        lastBrightness = brightness;
    }

    // Try to send data every minute
    if (millis() - timeRef > MINUTE || (buttonState == HIGH) ) {
        timeRef = millis();
        digitalWrite(yellow, HIGH);
        digitalWrite(green, LOW);
        digitalWrite(red, LOW);

        PRINT("Getting Temperatures");
        int count = getTemps();
        PRINT("Sending Temperatures");

        String postData = preparePostData(count, temps);
        PRINT(postData);
        if (post(postData)) {
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
        } else {
            delay(200);
        }
    }
}

String washerDataToJSON() {
    // We don't have sensors for washers 1 and 2 yet :(
    String washerData = String("\"access_key\":\"1bc7bbdc\", \"washers\": {");
    washerData += (washer3On) ? "\"w3\":1" : "\"w3\":0";
    washerData += "}";
    return washerData;
}

String preparePostData(int count, char temps[FEEDS][10]) {
    String postData = String("{\"temps\": {");

    for (int i = 0; i < FEEDS; ++i) {
        #if (DEBUG==1)
            Serial.print("Feed ");
            Serial.print(feeds[i]);
            Serial.print(" has value: ");
            PRINT(temps[i]);
        #endif

        postData += "\"" + feeds[i] + "\":";
        postData += temps[i];
        postData += (i == FEEDS - 1) ? "" : ", ";
    }
    postData += "}, " + washerDataToJSON();
    postData += "}";

   return postData;
}

//-----------------------------------------
//    POST data to server host
//    found at https://forum.arduino.cc/index.php?topic=145277.0
//
bool post(String thisData) {
    int inChar;
    if (!clientConnected) {  // if not connected then connect:
        connectToServer();
    }

    if (clientConnected) {
        client.println("POST / HTTP/1.1");  // send the header:
        client.println("Host: commonsense.qivc.org");
        client.println("Connection: close");
        client.println("Content-Type: application/json");
        client.print("Content-Length: ");
        client.println(thisData.length());
        client.println();
        client.print(thisData);    // send the data:
        PRINT("Sending data " + thisData);
    }
    else {
        PRINT("Heck, were're not connected!");
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
        if (client.connect("commonsense.qivc.org", 80)) {
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
