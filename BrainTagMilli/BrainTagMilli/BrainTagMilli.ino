/**
 *  +------------------------------------------------------+
 *  |      ____             _          ______              |
 *  |     / __ )_________ _(_)___     /_  __/___ _____ _   |
 *  |    / __  / ___/ __ `/ / __ \     / / / __ `/ __ `/   |
 *  |   / /_/ / /  / /_/ / / / / /    / / / /_/ / /_/ /    |
 *  |  /_____/_/   \__,_/_/_/ /_/    /_/  \__,_/\__, /     |
 *  |                                          /____/      | Milli() Version!
 *  +------------------------------------------------------+
 * Created: 19 July 2013 by Alex Rosengarten
 * Arduino Brain Library by Eric Mika, 2010
 * Last Update: 16 August 2013 
 *
 * This version of BrainTag attemps to time the game based on measuring time differences (via the milli() function)
 * instead of pausing the Arduino with the delay() function. The reason I attempt to do this is because when the 
 * Arduino uses delay, it cannot recieve any input or perform any other functions. With the milli() approach, it may
 * be possible to eliminate this problem, allowing, for instance, the Arduino to read an IR emitter while also 
 * blinking an IR LED. Though the microcontroller does not have serial capabilities (I'm using an Uno), this 
 * work-around would allow a single Arduino device to execute a better game laser tag where the players can shoot 
 * and be shot at nearly the same time.
 *
 * Since the last version:
 * - Changed settings for second headset
 * - Added 2nd irPin for IR LED and Laser
 * - Added tone melodies for state (i.e. off, attention, meditation) and laser/ir trigger
 */

#include <Brain.h>
#include "pitches.h"

// Set up the brain parser, pass it the hardware serial object you want to listen on.
Brain brain(Serial);

/* SETTINGS */
boolean DEBUG = true;

// Pin Settings
const byte irPin = 8;          // laser
const byte irPin2 = 2;         // IR LED
const byte rPin = 3;           // RGB LED Pins
const byte gPin = 5;
const byte bPin = 6;
const byte spPin =  7;          // Speaker Pin
const byte numMagPins = 3; const byte magPins[numMagPins] = {9, 10, 11};  // Magnitude indicators
const byte buttonPin = 12;     // Pushbutton pin
const byte sgPin = 13;         // Signal Quality Pin

  
const byte numStates = 3;               // Default: Off, Attention, Meditation
           // Possible States (Beta):   // 0 = off,   1 = attention,            2 = meditation, 
                                        // 3 = Delta, 4 = Alpha (High and Low), 5 = Beta (high and low), 
                                        // 6 = Gamma (Low and Mid)
                                        
const byte sampleSize = 10;             // Number of values used to calculate [level/trigger] (Beta)
const int fireDuration = 200;           // Time (ms) IR led fires. 
const float defaultFireThreshold = 80;   // A value between 0 - 100. Determines brink of impulse trigger.
/* END SETTINGS */

// Flags
boolean enteredLoop     = false;
String headsetStatus;
boolean sampleFullFlag  = false;

// Other
byte toggleCount = -1;
byte eegState = -1; 
byte eegData[numStates] = {};
byte eegSample[sampleSize] = {}; byte sample_index = 0; 

// The sum of a series of incrementing integers {1, 2, 3, ..., n-1, n} is equal to n^2/2 + n/2.
// Divide this by sample size (n) and you get n/2 + 1/2. 
long series_x = sampleSize/2+0.5;

float topMagReached = defaultFireThreshold;
unsigned float tmpMilli = 0;
unsigned long waitTime = 0;
unsigned long offTime = 0;
byte oldAttention;
byte oldMeditation;

// Tones
int offTone = NOTE_F4;
int atnTone[] = {NOTE_E6,NOTE_G6,NOTE_GS6};
int mdtnTone[] = {NOTE_DS6,NOTE_D6,NOTE_C6};
int fireTone[] = {NOTE_C7,NOTE_D7};

void setup() {
  Serial.begin(9600); 
  
  // Inputs
  pinMode(buttonPin, INPUT);

  // Outputs
  pinMode(rPin, OUTPUT);      pinMode(gPin, OUTPUT);      pinMode(bPin, OUTPUT);      // RBG LED
  pinMode(magPins[0],OUTPUT); pinMode(magPins[1],OUTPUT); pinMode(magPins[2],OUTPUT); // Magnitude Indicators
  pinMode(sgPin,OUTPUT);      pinMode(irPin,OUTPUT);           // Signal Quality & Laser 
  pinMode(irPin2,OUTPUT);     pinMode(spPin,OUTPUT);           // IR LED and Speaker
  if(DEBUG){
    // RBG pin test
    Serial.println("Test: RGB led should turn green now.");
    ledBlink(gPin,1,500);
    
    // Signal pin test
    Serial.println("Test: signal pin should turn on now.");
    ledBlink(sgPin,1,500);
    
    // Intensity pin(s) test
    Serial.println("Test: intensity pins should turn on now.");
    ledArrayBlink(magPins,1,500);
    
    // IR pin  test
    Serial.println("Test: IR pin should turn on now.");
    ledBlink(irPin,1,500);
    
    Serial.println("Ready.");
  } /* END DEBUG */
} /* END SETUP */

void loop() {
  if(enteredLoop == false && DEBUG){
    enteredLoop = true;  // Flag so that this condition is executed once
    toggleState(toggleCount++); // Initialize game to "off" mode
    Serial.println("Entered Loop");   
  } /* END DEBUG */


  // Toggle Game Mode via the Button Pin input
  if(digitalRead(buttonPin) == HIGH) {
    toggleState(toggleCount++);  
  } /* END TOGGLE GAME MODE */
  
  // Gather EEG Data whenever a packet of data is recieved from the headset. 
  if(brain.update()){
    if(DEBUG){
      // Serial.println(brain.readCSV);        // <-- Uncomment this to see all raw data values
      Serial.print(brain.readSignalQuality()); Serial.print(" ");
      Serial.print(brain.readAttention());     Serial.print(" ");
      Serial.print(brain.readMeditation());    Serial.print(" LB:");
      Serial.print(brain.readLowBeta());       Serial.print(" HB:");
      Serial.print(brain.readHighBeta());      Serial.print(" LA:");
      Serial.print(brain.readLowAlpha());      Serial.print(" HA:");
      Serial.println(brain.readHighAlpha());
    } /* END DEBUG */
    
     // Collect EEG data 
    eegData[0] = 0; // Resting state is set to 0.
    eegData[1] = brain.readAttention();
    eegData[2] = brain.readMeditation();
    
 /* Note: To read more data, you have to uncomment these lines *AND* change the "numStates" variable in settings 
    *AND* uncomment the cases in the toggleGame method below. 
    eegData[3] = brain.readDelta();
    eegData[4] = brain.readTheta();
    eegData[5] = brain.readLowAlpha();
    eegData[6] = brain.readHighAlpha();
    eegData[7] = brain.readLowBeta();
    eegData[8] = brain.readHighBeta();
    eegData[9] = brain.readLowGamma();
    eegData[10] = brain.readMidGamma();  
    */

    // Headset Status: This informs the user about the quality of the signal
    if(brain.readSignalQuality() == 200){
      headsetStatus = "off"; // Headset or Reference Electrode is not on. 
      ledBlink(sgPin,1,800);
    } else if (brain.readSignalQuality() != 200 && brain.readAttention() == 0) {
      headsetStatus = "noSig"; // Headset and Reference Electrode are on, but no signal is being picked up
      ledBlink(sgPin, 1, 600);
    } else if (headsetStatus == "on" && eegData[1] == oldAttention && eegData[2] == oldMeditation) {
      headsetStatus = "stuck"; // Headset is frozen or stuck: It recieves signal, but that signal is not new, probably due to a poor signal.
      ledBlink(sgPin, 1, 500);
    } else {
      // Game on!
      headsetStatus = "on"; 
      digitalWrite(sgPin,HIGH);
    } /* END HEADSET STATUS */
    
    if(DEBUG) Serial.println("Headset Status: " + headsetStatus);
    if(headsetStatus == "on") collectSample(eegData[eegState]); // This captures a few samples of eeg Data, used for trigger/level mechanism
  
    oldAttention   = eegData[1];   // Store old eeg data to test if brain actually updates. 
    oldMeditation  = eegData[2];
  } /* END BRAIN UPDATE */
  
    
  // Visualize EEG Data via Magnitude Indicator Pins
  intensityMeter(eegData[eegState]);  // Default is three mag pins 
  if(millis() >= offTime){ 
    digitalWrite(irPin,LOW);
  digitalWrite(irPin2,LOW);
  }
  // Method call to determine and execute a shot
  //determineIfFire(eegData[eegState]);
  
  // Cycle Toggle Count - so the game can keep track of unlimited game state changes.
  if(toggleCount == numStates) toggleCount = 0;     

} /* END LOOP */


/* METHODS */

/** determineIfFire(byte data)
 * 
 
void determineIfFire(byte data){
  float weight = 0;
  if(data >= topMagReached && headsetStatus == "on" && waitTime < millis()){
    topMagReached += (data - topMagReached) * weight;  // Incrementally adjust 
    ledBlink(irPin,1,fireDuration);
    waitTime = millis() + 500;
    Serial.print("Fired! data/topMagReached: ");
    Serial.print(data); Serial.print("  "); Serial.println(topMagReached);
  } 
} END determineIfFire */


/** collectSample(byte data)
 *
 */
void collectSample(byte data){
  eegSample[sample_index++] = data; // Set current value to current eegSample index, then increment the index
  if (sample_index == sampleSize-1){
    sample_index == 0;  // This makes eegSample into a circular array
    sampleFullFlag = true;
    
  }
  if(DEBUG && sampleFullFlag) for(int i = 0; i<sampleSize;i++)  Serial.print(eegSample[i]);
  
}

void ledArrayBlink(const byte pin[], int numBlinks, int del){
  int i = 0;
  byte j;
  unsigned long waitTime;
  while(i < numBlinks){
    for(j = 0; j < numMagPins; j++){        // j is a byte since numMagPins is also a byte datatype
      digitalWrite(pin[j],HIGH);
    } /* END FOR LOOP */
    delay(del);
    
    for(j = 0; j < numMagPins; j++){
      digitalWrite(pin[j],LOW);
    } /* END FOR LOOP */
    delay(del); 
    
    i++;
  } /* END WHILE LOOP */    
} /* END ledArrayBlink */

void ledBlink(byte pin, int numBlinks, int del){
  int i = 0;
  while(i < numBlinks){
    digitalWrite(pin,HIGH);
    delay(del);
    digitalWrite(pin,LOW);
    delay(del); 
    i++;
  } /* END WHILE LOOP */
} /* END ledBlink */
 

void intensityMeter(int mag){
  if(mag <= -1) ledBlink(rPin,2,200);
  byte ledPWR1 = map(constrain(mag, 00, 33), 00, 33, 0, 255);
  byte ledPWR2 = map(constrain(mag, 33, 66), 33, 66, 0, 255);
  byte ledPWR3 = map(constrain(mag, 66, 99), 66, 99, 0, 255);
  analogWrite(magPins[0],ledPWR1);
  analogWrite(magPins[1],ledPWR2);
  analogWrite(magPins[2],ledPWR3);
  
  if(ledPWR3 >= 100 && headsetStatus == "on" && waitTime < millis()){ 
    digitalWrite(irPin,HIGH);
    digitalWrite(irPin2,HIGH);
    offTime = millis() + 500;
    waitTime = millis() + 600;
    Serial.print("Fired! data/topMagReached: ");
    Serial.print(mag); Serial.print("  "); Serial.println(topMagReached);
 }
  
} /* END intensityMeter */

void toggleState(byte t) {
  int i;
  if(DEBUG) {
    Serial.print("Toggle Count is ");
    Serial.print(t);
    Serial.print(" and mod(t,number of states) is: ");
    Serial.println(t % numStates);
  } /* END DEBUG */
  
  eegState = t % numStates;
  
  switch (eegState){
    case 0: // OFF
      digitalWrite(rPin,HIGH);
      digitalWrite(gPin,LOW);
      digitalWrite(bPin,LOW);
      tone(spPin, offTone, 750);
      delay(975);
      noTone(spPin);
      if(DEBUG) Serial.println("State 0: Off - Red");
      break; 
      
    case 1: // ATTENTION
      digitalWrite(rPin,LOW);
      digitalWrite(gPin,HIGH);
      digitalWrite(bPin,LOW);
      for(i = 0; i < 3; i++){
        tone(spPin, atnTone[i], 250);
        delay(325);
        noTone(spPin);
      } /* END FOR LOOP */
      if(DEBUG) Serial.println("State 1: Attention - Green");
      break;
      
    case 2: // MEDITATION
      digitalWrite(rPin,LOW);
      digitalWrite(gPin,LOW);
      digitalWrite(bPin,HIGH);
      for(i = 0; i < 3; i++){
        tone(spPin, mdtnTone[i], 250);
        delay(325);
        noTone(spPin);
      } /* END FOR LOOP */
      if(DEBUG) Serial.println("State 2: Meditation - Blue");
      break;
  } /* END SWITCH CASE */
} /* END toggleState */

/* END METHODS & PROGRAM */


/* 
          _                 _           _      
        / /\              / /\        /\ \    
       / /  \            / /  \      /  \ \   
      / / /\ \          / / /\ \__  / /\ \ \  
     / / /\ \ \        / / /\ \___\/ / /\ \_\ 
    / / /  \ \ \       \ \ \ \/___/ / /_/ / / 
   / / /___/ /\ \       \ \ \    / / /__\/ /  
  / / /_____/ /\ \  _    \ \ \  / / /_____/   
 / /_________/\ \ \/_/\__/ / / / / /\ \ \     
/ / /_       __\ \_\ \/___/ / / / /  \ \ \    
\_\___\     /____/_/\_____\/  \/_/    \_\/    

*/

