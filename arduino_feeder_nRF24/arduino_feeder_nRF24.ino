#include <SPI.h>
#include "HX711.h"
#include "RF24.h"
#include "RF24Network.h"

#define secs *1000UL
#define mins *60 secs
#define hrs *60 mins
#define day *24 hrs

const int motorPin = 5;
const int foodSensor = 6;
const int foodLight = 7;
const int radioTestPin = 8;
const int CE = 9;
const int CSN = 10;
// HX711_SCK 2 , HX711_DT 3
// nRF24L01 set up on the nano's SPI bus... 9-CE, 10-CSN, 11-MO, 12-MI, 13-SCK

const char msg_RESET[] = "reset";
const char msg_READY[] = "ready";
const char msg_FILLED[] = "bowl filled";
const char msg_NEAREMPTY[] = "near empty - top it up!";
const char msg_FAILEDFILL[] = "bowl was not filled after a minute; check food/blockage";
const char msg_RADIOTEST[] = "radio test";
const char msg_DEFAULTTIME[] = "default wait time elapsed without any msgs from server";

int defaultAmount = 30;  // grams
int currentWeight = 0;

unsigned long lastFeedTime = 0;
unsigned long lastSampleTime = 0;
unsigned long attemptTime = 60 secs;
unsigned long defaultWait = 1 day + 1 hrs;
unsigned long sampleWait = 5 mins;

const int nodeID = 2;  // unique nodes required for each project
RF24 radio(CE,CSN);
RF24Network network(radio);

/////////////////////////////////////////////////////////////////////////////

void setup(void) {
  Serial.begin(57600);
  Serial.println("Setup began");
  Init_Hx711();
  delay(3000);
  Get_Maopi();
  
  SPI.begin();
  radio.begin();
  network.begin(90, nodeID);
  
  pinMode(motorPin, OUTPUT);
  pinMode(foodSensor, INPUT);
  pinMode(foodLight, OUTPUT);
  pinMode(radioTestPin, INPUT_PULLUP);
  
  #ifdef DEBUG
    digitalWrite(motorPin, HIGH);
    delay(1000);
    digitalWrite(motorPin, LOW);
  #endif

  transmit(msg_RESET, sizeof(msg_RESET));
  Serial.println("s <weight>     set default output amount");
  Serial.println("g              get default output amount");
  Serial.println("o              output default amount");
  Serial.println("o <weight>     output the given amount");
  Serial.println("r              read sensor");
  Serial.println("c              clear sensor");
  Serial.println(">              drive screw forwards");
  Serial.println("<              drive screw backwards");
  Serial.print("System started. Current weight: ");
  Serial.println(Get_Weight());
}


void loop() {
  network.update();

  while (digitalRead(radioTestPin) == LOW) {
    if (transmit(msg_RADIOTEST, sizeof(msg_RADIOTEST)))
      Serial.println("radio test succeeded");
    else
      Serial.println("radio test failed");
    delay(1000);
  }

  // check how much has been eaten
  // TODO - add a delay to make sure we aren't reading when the scale is being pushed down
  if (uint32_t(millis() - lastSampleTime) > sampleWait) {
    int eaten = currentWeight - sensor();
    if (eaten >= 1) {
      char msg[40];
      sprintf(msg, "eaten %i, remaining %i", eaten, sensor());
      Serial.println(msg);
      transmit(msg, sizeof(msg));
    }
    lastSampleTime = millis();
  }
      
  // check if it needs topped up
  if (foodSensor == LOW) {
    Serial.println(msg_NEAREMPTY);
    transmit(msg_NEAREMPTY, sizeof(msg_NEAREMPTY));
  }

  if (uint32_t(millis() - lastFeedTime) > defaultWait) {
    Serial.println(msg_DEFAULTTIME);
    transmit(msg_DEFAULTTIME, sizeof(msg_DEFAULTTIME));
    output(defaultAmount);
  }
  
  checkSerialForInput();
  checkRadioForInput();
}// end of loop

/////////////////////////////////////////////////////////////////////////////

void output(int amount) {
  char msg[80];
  sprintf(msg, "outputting %i grams", amount);
  Serial.println(msg);
  transmit(msg, sizeof(msg));

  unsigned long startingTime = millis();
  int startingWeight = sensor();
  while (sensor() < (startingWeight + amount)) {
    digitalWrite(motorPin, HIGH);
    delay(600);
    digitalWrite(motorPin, LOW);
    delay(200);
    printWeight();
    //timeout
    if (uint32_t(millis() - startingTime) > attemptTime) {
      Serial.println(msg_FAILEDFILL);
      transmit(msg_FAILEDFILL, sizeof(msg_FAILEDFILL));
      break;
    }
  }
  delay(1000); // let the kibble settle
  printWeight();
  currentWeight = sensor();
  sprintf(msg, "startingWeight: %i || outputAmount: %i || target: %i || actual: %i", startingWeight, amount, startingWeight+amount, currentWeight);
  Serial.println(msg);
  transmit(msg, sizeof(msg));
}

void printWeight() {
  Serial.print("Current weight: ");
  Serial.println((int)Get_Weight());
}

int sensor() {
  return (int)Get_Weight(); // needs a cast as Get_Weight() returns unsigned int
}

void checkSerialForInput() {
  String s = "";
  while(Serial.available() > 0) {
    delay(20);
    s.concat(char(Serial.read()));
  }
  if (s != "")
    processInput(s);
}

void checkRadioForInput() {
  String s = "";
  if (network.available()) {
    delay(50);
    RF24NetworkHeader header;
    network.peek(header);
    if(header.type == 'F') {
      char msg[50];
      network.read(header, &msg, 50);
      s = String(msg);
      //Serial.print("received via radio: ");
      //Serial.println(s);
    }
  }
  if (s != "")
    processInput(s.substring(s.indexOf(" ")+1)); // strip the FEEDER prefix
}

void processInput(String s) {
  switch(s.charAt(0)) {
    case 's': {
      defaultAmount = (s.substring(s.indexOf(" "))).toInt();
    } // fall through
    case 'g': {
      char msg[50];
      sprintf(msg, "default amount set to: %i", defaultAmount);
      Serial.println(msg);
      transmit(msg, sizeof(msg));
      break;
    }
    case 'o': {
      if (s.length() == 1)
        output(defaultAmount);
      else
        output(  (s.substring(s.indexOf(" "))).toInt()  );
      break;
    }
    case 'r': {
      char msg[50];
      sprintf(msg, "current weight:  %i", sensor());
      Serial.println(msg);
      transmit(msg, sizeof(msg));
      break;
    }
    case 'c': {
      Get_Maopi();
      break;
    }
    case '>': {
      digitalWrite(motorPin, HIGH);
      delay(600);
      break;
    }
    case '<': {
      digitalWrite(motorPin, HIGH);
      delay(600);
      break;
    }        
    default:
      break;
  }
}

bool transmit(const char msg[], int msgsize) {
  network.update();
  RF24NetworkHeader header(0, 'F');
  
  int count = 1;
  bool ok = false;
  while (count <= 3 && !ok) {
    Serial.print("Sending: ");
    Serial.print(msg);
    ok = network.write(header, msg, msgsize);
    if (ok)
      Serial.println(" ... successfull! ");
    else
      Serial.println(" ... failed!");
    count++;
  }
  return ok;
}
