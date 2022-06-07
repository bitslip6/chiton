#include <Wire.h>
/*********************************************************
Quad-Wheel Mecanum Vectoring Robot with Encder Input
Brent Clay / Angel Diaz
SuperDroid Robots
October 28, 2016
This code uses an Arduino Uno mounted on a Quad-Wheel Mecanum Vectoring
platform (TP-152-004, TP-095-004, or TP-252-004). The Arduino commands
two dual channel Sabertooth motor controllers to drive four independent
Mecanum wheels. Sabertooths are set to serial mode with addresses 128
and 129.
This firmware allows vectoring RC control of the robot's motion
Code is written for a Spektrum remote + receiver but could easily be reused
for other RC approaches
Platforms:
http://www.superdroidrobots.com/shop/item.aspx/programmable-mecanum-wheel-vectoring-robot-ig52-db/1788/
http://www.superdroidrobots.com/shop/item.aspx/programmable-mecanum-wheel-vectoring-robot-ig32-sb/1713/
http://www.superdroidrobots.com/shop/item.aspx/programmable-mecanum-wheel-vectoring-robot-ig32-dm/1487/
Motor Controller:
http://www.superdroidrobots.com/shop/item.aspx/sabertooth-dual-12a-motor-driver/1155/
Vectoring Robot Support Page:
http://www.superdroidrobots.com/shop/custom.aspx/vectoring-robots/44/
Spektrum DX5e:
http://www.superdroidrobots.com/shop/item.aspx/spektrum-dx5etransmitter-with-ar610-receiver/992/
***********************************************************/
// ****************************************************
// Libraries
// ****************************************************
//
#include "src/Roboteq/Roboteq.h"
#include <SoftwareSerial.h>
#include <Wire.h>

// *********************
// Define hardware pins
// *********************
// RC mappings -- strafe: aileron, drive: elevation, turn: rudder
int strafePinRC = 13, drivePinRC = 12, turnPinRC = 11;
#define PULSEIN_TIMEOUT 10000

// *********************
// RC Vars
// *********************
unsigned long DRIVE_PULSE_WIDTH;
unsigned long TURN_PULSE_WIDTH;
unsigned long STRAFE_PULSE_WIDTH;
float pulseLow = 1051, pulseHigh = 1890;
int rc_offset = 0;
// RoboteQ motor controller
SoftwareSerial debugSerial(7,6);
Roboteq roboteq(&Serial1);
//long commandMotors(float drive,float strafe,float turn);      // Variables used to command the motors
long encFL = 0, encFR = 0, encRL = 0, encRR = 0;
unsigned short min_dist = 601;
void setup() {
  debugSerial.begin(38400);  //debug output
  Serial1.begin(38400);  //Roboteq
  // Initialize I2C communications as Slave
  Wire.begin(0x21);
  Wire.onReceive(receiveEvent);
  roboteq.allStop();
  pinMode(drivePinRC, INPUT);
  pinMode(turnPinRC, INPUT);
  pinMode(strafePinRC, INPUT);
  delay(3000);
  rc_offset = pulseIn(drivePinRC, HIGH) - 1500;
  Serial.print(rc_offset);
}
void readEncoders()
{
  // have to break this up since Arduino Serial buffer is only 64 bytes
  // just used to send encoder request commands over CAN
  while(Serial1.available()) { Serial1.read(); }
  Serial.println("@01?C 1"); delay(5); roboteq.readCAN(&debugSerial, false);
  Serial.println("@01?C 2"); delay(5); roboteq.readCAN(&debugSerial, false);
  Serial.println("@02?C 1"); delay(5); roboteq.readCAN(&debugSerial, false);
  Serial.println("@02?C 2"); delay(8); roboteq.readCAN(&debugSerial, false);
  Serial.print("\n");
  if(roboteq.encRx_11) { /*debugSerial.println("enc 11 rx!");*/ encFL = roboteq.enc11; roboteq.encRx_11 = false; }
  if(roboteq.encRx_12) { /*debugSerial.println("enc 12 rx!");*/ encFR = roboteq.enc12; roboteq.encRx_12 = false; }
  if(roboteq.encRx_21) { /*debugSerial.println("enc 21 rx!");*/ encRL = roboteq.enc21; roboteq.encRx_21 = false; }
  if(roboteq.encRx_22) { /*debugSerial.println("enc 22 rx!");*/ encRR = roboteq.enc22; roboteq.encRx_22 = false; }
}
void loop() {
/*
  for(int i=0; i<4; i++) {
   // commandMotors(0, 0, 0);
    readEncoders();  
    // debug print
    debugSerial.print(encFL); debugSerial.print(","); debugSerial.print(encFR);
    debugSerial.print(","); debugSerial.print(encRL); debugSerial.print(","); debugSerial.print(encRR); 
    debugSerial.println();
    delay(100);
  }*/
  // If you want to drive th robot via rc un comment next statements.
  // Read in the RC pulses
  DRIVE_PULSE_WIDTH = rcCheck(pulseIn(drivePinRC, HIGH) -  rc_offset);//, PULSEIN_TIMEOUT);
  TURN_PULSE_WIDTH = rcCheck(pulseIn(turnPinRC, HIGH) -  rc_offset);//, PULSEIN_TIMEOUT);
  STRAFE_PULSE_WIDTH = rcCheck(pulseIn(strafePinRC, HIGH) -  rc_offset);//, PULSEIN_TIMEOUT);
  // If pulses too short, stop motors
  if(DRIVE_PULSE_WIDTH < 500 || TURN_PULSE_WIDTH < 500 || STRAFE_PULSE_WIDTH < 500) {
    roboteq.allStop();
    return;
  }
  // convert RC signals to continuous values from [-1000,1000]
  float driveVal = convertRCToInt(DRIVE_PULSE_WIDTH);
  float turnVal =  convertRCToInt(TURN_PULSE_WIDTH);
  float strafeVal = convertRCToInt(STRAFE_PULSE_WIDTH);
  // limit forward drive when object is detected
  if (min_dist < 600 && driveVal > 0) {
    driveVal = 0;
  }
  //mixing
  int motorFR = checkCommand(driveVal + turnVal + strafeVal);
  int motorRR = checkCommand(driveVal + turnVal - strafeVal);
  int motorFL = checkCommand(driveVal - turnVal - strafeVal);
  int motorRL = checkCommand(driveVal - turnVal + strafeVal);
  // command motors
  roboteq.commandCAN(motorFL, motorFR, 1);
  roboteq.commandCAN(motorRR, motorRL, 2);
  Serial.print("driveVal: ");Serial.print(driveVal); Serial.print("\t");
  Serial.print("turnVal: ");Serial.print(turnVal); Serial.print("\t");
  Serial.print("strafeVal: "); Serial.print(strafeVal); Serial.print("\n");
  Serial.print("motorFR: "); Serial.print(motorFR); Serial.print("\t");
  Serial.print("motorFL: "); Serial.print(motorFL); Serial.print("\t");
  Serial.print("motorRR: "); Serial.print(motorRR); Serial.print("\t");
  Serial.print("motorRL: "); Serial.print(motorRL); Serial.print("\n");
  Serial.print("**** min_dist to robot: "); Serial.print(min_dist);Serial.print("  *****");Serial.print("\n");
  //mcSerial.print("EOF");  //realterm sync
  /*
  debug print rc input
  Serial.print(DRIVE_PULSE_WIDTH); Serial.print(","); Serial.print(TURN_PULSE_WIDTH);
  Serial.print(","); Serial.println(STRAFE_PULSE_WIDTH);
  Serial.print("\t");
Serial.print(motorFL,DEC);  Serial.print(","); Serial.print(motorFR,DEC);  Serial.print(",");
  Serial.print(motorRL,DEC);  Serial.print(","); Serial.print(motorRR,DEC);
  Serial.print("\n");
  */
delay(10);
}
int convertRCToInt(unsigned long value)
{
  int checkVal = map(value, 1050, 1950, -1000, 1000);
  checkVal = checkVal < -1000 ? -1000 : checkVal;
  checkVal = checkVal >  1000 ? 1000 : checkVal;
  return checkVal;
}
int rcCheck(int val){
  if (val < 900 || val > 2100){ return 0; }
  else if (val > 1450 && val < 1550) { val = 1500; }
  return val;
}
int checkCommand(int val){
  val = val < -1000 ? -1000 : val;
  val = val > 1000 ? 1000 : val;
  return val;
}
void receiveEvent() {
  uint16_t n = Wire.available();
  while (n >= 2) {
    byte input[n];
    Wire.readBytes(input, n);
    min_dist = input[1] | (input[0] << 8);
    n-=2;
  }
} 