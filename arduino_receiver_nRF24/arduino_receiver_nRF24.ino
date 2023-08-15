
#include <SPI.h>
#include "RF24.h"
#include "RF24Network.h"

RF24 radio(9,10);
RF24Network network(radio);
// nRF24L01 set up on the nano's SPI bus... 9-CE, 10-CSN, 11-MO, 12-MI, 13-SCK,

void setup(void) {
  delay(100);
  SPI.begin();
  radio.begin();
  network.begin(90, 0);
  Serial.begin(57600);
}

void loop(void) {

  network.update();

  // handle incoming radio msgs
  if (network.available()) {
    delay(50);
    RF24NetworkHeader header;
    network.peek(header);

    if(header.type == 'W') {
      char temp[50];
      network.read(header, &temp, 50);    
      Serial.print("FOUNTAIN: ");
      Serial.println(temp);
    }
    if(header.type == 'F') {
        // other nRF24 projects
	// setup with unique headers
    }
  }// end radio of handler

  
  // handle serial msgs to send instructions to project nRF24s
  if (Serial.available() > 0) {
    String s = "";
    while (Serial.available() > 0) {
      delay(20);
      s += char(Serial.read());
    }

    // code related to other projects
    
    
    }
  }// end serial handler
  
}