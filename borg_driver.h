#ifndef __BORG_DRIVER_H
#define __BORG_DRIVER_H 1


void random_direction();
void *random_drive(void *foo);
void BorgInit();
void FailSafe();
void Wiggle();
void AllStop();
void Left90();
void Right90();
void Right();
void Left();
void DriveBackward(int power);
void DriveForward(int power);
void ReadBattery();


#endif