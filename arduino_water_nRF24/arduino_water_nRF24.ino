#include <SPI.h>
#include "RF24.h"
#include "RF24Network.h"
#include "my_vars.h"

////////////////////////////////////////////

#define secs *1000UL
#define mins *60 secs
#define hrs *60 mins

unsigned long bowlWait = 5 mins;          // time to wait for bowl to return
unsigned long evaporationWait = 25 mins;  // time to wait for water to evaporate before filling
unsigned long waterSampleWait = 5 mins;   // duration between taking water samples
unsigned long waterFillingTime = 25 secs; // changed in CALIBRATING mode
const unsigned long maxTimeOn = 30 secs;  // max time the valve will be on in a single WATERFILLING event
const unsigned long minTimeOn = 10 secs;  // min time the valve will be on in a single WATERFILLING event
const unsigned long expFillGap = 6 hrs;   // min 6hrs expected between fills, for warning purposes
const unsigned long debugPrintWait = 1 secs;   // time to wait to output debug msgs
const unsigned long armLoweringWait = 10 secs; // time to wait for sensor arm to be lowered

const int ledR = 7;
const int ledG = 5;
const int ledB = 6;
const int bowlSensor = 2; // INT0 is pin2
const int valve = 4;
const int waterVcc = A0;
const int waterSensor = A1;
const int calibrationPin = A2;
const int radioTestPin = A3;
const int connectedLed = A4;
const int connectedPin = 3;
const int debugSelect = 8;
const int CE = 9;
const int CSN = 10;
// unused pins A5, A6, A7
// nano PWM pins: 3, 5, 6, 9, 10, 11
// nRF24L01 set up on the nano's SPI bus... 9-CE, 10-CSN, 11-MO, 12-MI, 13-SCK

const char msg_FOUNTAINRESET[] = "fountain reset";
const char msg_READY[] = "fountain ready";
const char msg_BOWLREMOVED[] = "bowl removed";
const char msg_BOWLMISSING[] = "bowl missing > 5mins";
const char msg_BOWLREPLACED[] = "bowl replaced";
const char msg_BOWLEMPTY[] = "bowl empty";
const char msg_FILLED[] = "bowl filled";
const char msg_FILLEDTOOSOON[] = "filled again too soon";
const char msg_CHECKWATERSENSOR[] = "check water sensor";
const char msg_CALIBRATED[] = "fill time calibrated";
const char msg_RADIOTEST[] = "radio test";
const char msg_DISCONNECTED[] = "fountain disconnected";
const char msg_CONNECTED[] = "fountain connected";

boolean msgDisconnectedSent = false;
boolean msgRemovedSent = false;
boolean msgMissingSent = false;
boolean msgFilledTooSoonSent = false;
boolean msgErrorSent = false;

volatile State state;
volatile unsigned long timeStamp = 0;     
volatile unsigned long lastInterrupt = 0;
volatile int countInterrupts = 0;
unsigned long lastFillTime = 0;           // time since the bowl was last filled
unsigned long lastWaterSampleTime = 0;    // time since the water was last sampled
unsigned long lastDebugOutput = 0;        // time since the debug msg was last printed

const int nodeID = 1;  // unique nodes required for each project
RF24 radio(CE,CSN);
RF24Network network(radio);

////////////////////////////////////////////

void setup() {
  Serial.begin(57600);
  Serial.println("System started");

  SPI.begin();
  radio.begin();
  network.begin(90, nodeID);

  delay(100);
  pinMode(valve, OUTPUT);
  pinMode(waterVcc, OUTPUT);
  digitalWrite(valve, LOW); // make sure valve is off
  digitalWrite(waterVcc, LOW);

  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);
  pinMode(connectedLed, OUTPUT);
  pinMode(connectedPin, INPUT_PULLUP);
  pinMode(calibrationPin, INPUT_PULLUP);
  pinMode(bowlSensor, INPUT_PULLUP);
  pinMode(radioTestPin, INPUT_PULLUP);
  pinMode(debugSelect, INPUT_PULLUP);
  pinMode(waterSensor, INPUT); // using external 1Mohm pullup

  attachInterrupt(0, whereIsMyBowl, CHANGE); // INT0 is pin2
  attachInterrupt(1, whereIsMyBowl, CHANGE); // INT1 is pin3
  
  transmit(msg_FOUNTAINRESET, sizeof(msg_FOUNTAINRESET));
  digitalWrite(connectedLed, HIGH);
  if (!isFountainConnected() || !isBowlPresent())
    whereIsMyBowl();
  else                   // could just set it to start from DISCONNECTED
    setState(ARMWAIT);   // but doing this avoids the extra prints & emails being sent
  Serial.println("Setup finished");
}

////////////////////////////////////////////

void loop() {
  
  if (digitalRead(debugSelect) == LOW) {
    if (uint32_t(millis() - lastDebugOutput) > debugPrintWait)
      printStatus(); // particularly handy to check the waterSensor sensitivity
  }

  switch(state) {

    case DISCONNECTED: {
      setColour(BLACK);
      digitalWrite(valve, LOW);
      digitalWrite(waterVcc, LOW);
      digitalWrite(connectedLed, LOW);
      if (!msgDisconnectedSent) { // limit msgs to one per event
          transmit(msg_DISCONNECTED, sizeof(msg_DISCONNECTED));
          msgDisconnectedSent = true;
      }
      if (isFountainConnected()) {
        delay(100); // guard against accidental presses
        if (isFountainConnected) { // fountain definitely reconnected
          transmit(msg_CONNECTED, sizeof(msg_CONNECTED));
          msgDisconnectedSent = false; // reset
          digitalWrite(connectedLed, HIGH);
          setState(BOWLMISSING);
        }
      }
      break;
    }    
    
    case BOWLMISSING: {
      digitalWrite(valve, LOW);
      digitalWrite(waterVcc, LOW);
      unsigned long elapsedTime = millis() - timeStamp;
      if (elapsedTime <= bowlWait) {
        setColour(YELLOW);
        if (!msgRemovedSent) { // limit msgs to one per event
          transmit(msg_BOWLREMOVED, sizeof(msg_BOWLREMOVED));
          msgRemovedSent = true;
        }
      }
      if (elapsedTime > bowlWait) {
        setColour(RED);
        if (!msgMissingSent) { // limit msgs to one per event
          transmit(msg_BOWLMISSING, sizeof(msg_BOWLMISSING));
          msgMissingSent = true;
        }
      }
      if (isBowlPresent()) {
        delay(100); // guard against accidental presses
        if (isBowlPresent()) { // bowl definitely back
          transmit(msg_BOWLREPLACED, sizeof(msg_BOWLREPLACED));
          msgRemovedSent = false; // reset
          msgMissingSent = false; // reset
          setState(ARMWAIT);
        }
      }
      break;
    }

    case ARMWAIT: {
      // the bowl has just been replaced and it's likely empty...
      // lets not wait for evaporationWait, instead
      // wait X seconds to allow the arm to be lowered and then check water level
      setColour(WHITE);
      if (uint32_t(millis() - timeStamp) > armLoweringWait) {
        if (isWaterEmpty() && isBowlPresent()) {
          transmit(msg_BOWLEMPTY, sizeof(msg_BOWLEMPTY));
          setState(WATERFILLING);
        }
        else {
          transmit(msg_READY, sizeof(msg_READY));
          setState(READY);
        }
      }
      break;
    }
    
    case WATEREMPTY: {
      setColour(MAGENTA);
      if (!isWaterEmpty()) {
        transmit(msg_READY, sizeof(msg_READY));
        setState(READY);
        break;
      }
      if (uint32_t(millis() - lastFillTime) < expFillGap) {
        if (!msgFilledTooSoonSent) { // limit msgs to one per event
          transmit(msg_FILLEDTOOSOON, sizeof(msg_FILLEDTOOSOON));
          msgFilledTooSoonSent = true;
        }
      }
      if (uint32_t(millis() - timeStamp) >= evaporationWait) {
        msgFilledTooSoonSent = false; // reset
        setState(WATERFILLING);
      }
      break;
    }
    
    case WATERFILLING: {
      setColour(CYAN);
      if (uint32_t(millis() - timeStamp) < waterFillingTime)
        digitalWrite(valve, HIGH);
      else {
        digitalWrite(valve, LOW);
        lastFillTime = millis();
        timeStamp = millis();
        transmit(msg_FILLED, sizeof(msg_FILLED));
        if (isWaterEmpty())  // check the sensor is picking up the fresh water
          setState(ERRORS);
        else {
          transmit(msg_READY, sizeof(msg_READY));;
          setState(READY);
        }
      }
      break;
    }
    
    case CALIBRATING: {
      setColour(GREEN);
      digitalWrite(valve, HIGH);
      unsigned long elapsedTime = millis() - timeStamp;
      if (digitalRead(calibrationPin)) {    // if pin gone back to high
        delay(100);
        if (digitalRead(calibrationPin)) {  // debounce
          waterFillingTime = elapsedTime;
          if (elapsedTime > maxTimeOn)      // ensure we can't set it to overfill
            waterFillingTime = maxTimeOn;        
          if (elapsedTime < minTimeOn)      // ensure we can't set it too small
              waterFillingTime = minTimeOn;
          digitalWrite(valve, LOW);
          lastFillTime = millis();
          transmit(msg_CALIBRATED, sizeof(msg_CALIBRATED));
          setState(READY);
        }
      }
      break;
    }
    
    case RADIOTEST: {
      if (!transmit(msg_RADIOTEST, sizeof(msg_RADIOTEST))) {
        setColour(ORANGE);
        delay(500);
        setColour(PURPLE);
        delay(500);
      }
      else {
        setColour(ORANGE);
        delay(500);
        setColour(GREEN);
        delay(500);
      }
      if (digitalRead(radioTestPin)) // pin gone back to high
        state = READY; // set it directly so it doesn't print or reset the timestamp
      break;
    }
    
    case READY: {
      digitalWrite(valve, LOW);
      digitalWrite(waterVcc, LOW);
      setColour(BLUE);
      if (!isFountainConnected()) {
        Serial.println("  -- WE CAUGHT A DISCONNECTED EVENT NOT USING THE INTERRUPT.");
        setState(DISCONNECTED);
        break;
      }
      if (!isBowlPresent()) {
        Serial.println("  -- WE CAUGHT A BOWLMISSING EVENT NOT USING THE INTERRUPT.");
        setState(BOWLMISSING);
        break;
      }
      if (isCalibrating()) {
        setState(CALIBRATING);
        break;
      }
      if (isRadioTesting()) {
        state = RADIOTEST; // set it directly so it doesn't print or reset the timestamp
        break;
      }
      if (uint32_t(millis() - lastWaterSampleTime) > waterSampleWait) {
        lastWaterSampleTime = millis();
        if (isWaterEmpty()) {
          transmit(msg_BOWLEMPTY, sizeof(msg_BOWLEMPTY));
          setState(WATEREMPTY);
          break;
        }
      }
      break;
    }
    
    case ERRORS: {
      if (!msgErrorSent) {
        detachInterrupt(0);
        detachInterrupt(1);
        transmit(msg_CHECKWATERSENSOR, sizeof(msg_CHECKWATERSENSOR));
        msgErrorSent = true;
      }
      delay(500);
      setColour(RED);
      delay(500);
      setColour(BLACK);
      // reaching this state indicates the water was filled and the sensor didn't pick it up
      // possible cause 1; the water sensor was left raised up
      // possible cause 2; the water sensor wires have corroded and need replaced
      // need to reset the power if we reach this state
    }
    
    default:
      break;
  }// end of switch
}// end of loop

// interrupt routine
void whereIsMyBowl() {
  digitalWrite(valve, LOW);
  digitalWrite(waterVcc, LOW);
  
  if ((millis() - lastInterrupt) > 100) {
    if (!isFountainConnected())
      state = DISCONNECTED;
    else
      state = BOWLMISSING;

    timeStamp = millis();
    lastInterrupt = millis();
    countInterrupts++;
  }  
  // putting in the debounce logic has the effect of reducing multiple triggering
  // but the valve output isn't always forced to low as we would expect
  // therefore; copied the digitalWrites up to the switch case which ensures they are turned off
  // ... left them in this function anyway since it can't hurt
}

void setState(State s) {
  state = s;
  timeStamp = millis();
  printStatus();
}

////////////////////////////////////////////

boolean isBowlPresent() {
  return digitalRead(bowlSensor) == LOW ? true : false;
  // bowl pin pulled high by default
  // bowl switch is wired "normally open"
  // when bowl is present; switch is made; pin is low
  // when bowl is removed; switch is broken; pin is high
}

boolean isFountainConnected() {
  return digitalRead(connectedPin) == LOW ? true : false;
}

boolean isWaterEmpty() {
  int sample = getWaterSample();
  return sample < 150 ? false : true;
  // water pin pulled high temporarily using another outout pin and external resistor
  // water present; line is shorted; pin is pulled towards ground
}

boolean isCalibrating() {
  return digitalRead(calibrationPin) == LOW ? true : false;
}

boolean isRadioTesting() {
  return digitalRead(radioTestPin) == LOW ? true : false;
}

int getWaterSample(){
  digitalWrite(waterVcc, HIGH);
  delay(5);
  int sensorValue = analogRead(waterSensor);
  delay(5);
  digitalWrite(waterVcc, LOW);
  return sensorValue;
}

void setColour(const int colour[]){
  analogWrite(ledR, colour[0]);
  analogWrite(ledG, colour[1]);
  analogWrite(ledB, colour[2]);
}

void printStatus() {
  lastDebugOutput = millis();
  Serial.print("state: ");
  switch(state) {
    case BOWLMISSING:   Serial.print("BOWLMISSING"); break;
    case WATEREMPTY:    Serial.print("WATEREMPTY"); break;
    case WATERFILLING:  Serial.print("WATERFILLING"); break;
    case ARMWAIT:       Serial.print("ARMWAIT"); break;
    case READY:         Serial.print("READY"); break;
    case CALIBRATING:   Serial.print("CALIBRATING"); break;
    case RADIOTEST:     Serial.print("RADIOTEST"); break;
    case DISCONNECTED:  Serial.print("DISCONNECTED"); break;
    case ERRORS:        Serial.print("ERRORS"); break;
  }
  Serial.print("  ||  waterSensor: ");
  Serial.print(getWaterSample(), DEC);
  Serial.print("  ||  waterFillingTime: ");
  Serial.print(waterFillingTime, DEC);
  Serial.print("  ||  lastFillTime: ");
  Serial.print(lastFillTime, DEC);
  Serial.print("  ||  interrupts: ");
  Serial.print(countInterrupts, DEC);
  Serial.print("  ||  bowlSensor: ");
  Serial.print(digitalRead(bowlSensor));
  Serial.print("  ||  connectedPin: ");
  Serial.print(digitalRead(connectedPin));
  Serial.print("  ||  timeStamp: ");
  Serial.println(timeStamp, DEC);
}

bool transmit(const char msg[], int msgsize) {
  network.update();
  RF24NetworkHeader header(0, 'W');
  
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
