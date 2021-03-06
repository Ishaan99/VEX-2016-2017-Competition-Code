#pragma config(I2C_Usage, I2C1, i2cSensors)
#pragma config(Sensor, in7,    pincerPos,      sensorPotentiometer)
#pragma config(Sensor, in8,    armPos,         sensorPotentiometer)
#pragma config(Sensor, I2C_1,  driveBL,        sensorQuadEncoderOnI2CPort,    , AutoAssign )
#pragma config(Sensor, I2C_2,  driveBR,        sensorQuadEncoderOnI2CPort,    , AutoAssign )
#pragma config(Motor,  port1,           driveFL,       tmotorVex393_HBridge, openLoop, driveLeft)
#pragma config(Motor,  port2,           driveBL,       tmotorVex393_MC29, openLoop, driveLeft, encoderPort, I2C_1)
#pragma config(Motor,  port3,           pincerLeft,    tmotorVex393_MC29, openLoop, reversed)
#pragma config(Motor,  port4,           liftTL,        tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port5,           liftBL,        tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port6,           pincerRight,   tmotorVex393_MC29, openLoop)
#pragma config(Motor,  port7,           liftTR,        tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port8,           liftBR,        tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port9,           driveBR,       tmotorVex393_MC29, openLoop, reversed, driveRight, encoderPort, I2C_2)
#pragma config(Motor,  port10,          driveFR,       tmotorVex393_HBridge, openLoop, reversed, driveRight)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

/* DO NOT MODIFY */
#pragma platform(VEX2)
#pragma competitionControl(Competition)
#include "Vex_Competition_Includes.c"
// Include the lcd button get utility function
#include "getlcdbuttons.c"
/*****************/

/*-----------------------------------------------------------------------------*/
/*  Definition of the menus and global variables for the autonomous selection  */
/*-----------------------------------------------------------------------------*/

typedef enum {
	kSideLeft = 0,
	kSideRight
} vexSide;

typedef enum {
	kStartHanging = 0,
	kStartMiddle
} vexStartposition;

typedef enum {
	kMenuStart    = 0,

	kMenuSide = 0,
	kMenuStartpos,
	kMenuAutonSelect,

	kMenuMax
} vexLcdMenus;

static  vexSide             vSide = kSideLeft;
static  vexStartposition    vStartPosition = kStartHanging;
static  short               vAuton = 0;

/*---------------------------------------------------------------------------*/
/*																			 */
/*			            Constant Declarations       	    	             */
/*																			 */
/*---------------------------------------------------------------------------*/

//Arm Potentiometer Constant Positions
const int armPrimed = 1200; //Height to knock off stars

//Pincer Potentiometer Constant Positions
const int pincerFlush = 3160; // Flat. To knock off stars from fence

//Arm PD Control Loop Constants
const float kPArm = 0.4; //Proportional Constant
const float kDArm = 0.9; //Derivative Constant

//Pincer P Control Loop Constants
const float kPPincer = 0.5; //Proportional Constant

/*---------------------------------------------------------------------------*/
/*															  */
/*			            Variable Declarations                             */
/*															  */
/*---------------------------------------------------------------------------*/

//Drive Var
float ticksPerRevDFWD = 627; //Ticks in one rotation of High Torque motor
int driveThres = 7; //Threshold for joystick input
//Arm Var
int armThres = 80; //Number potentiometer reading can be off by in Arm PD Control Loop
//Pincer Var
int pincerThres = 30; //Number potentiometer reading can be off by in Pincer P Control Loop

float goToClawPos; //Stores the position the claw should go to
float goToArmPos; //Stores the position the arm should go to
float driveStraightDistance; //Stores how far the drive should move (inches)

/*---------------------------------------------------------------------------*/
/*															  */
/*                           Function Declarations                           */
/*															  */
/*---------------------------------------------------------------------------*/

void drive(float distance); //Drive function in inches
void driveStop(); //Function to stop the drive
//Functions to move sub-systems at a given power
void moveArm(float pwr);
void movePincer(float pwr);
void driveLeftSide(float pwr);
void driveRightSide(float pwr);
//Autonomous LCD Picker Functions
void LcdAutonomousDisplay(vexLcdMenus menu);
void LcdAutonomousSelection();
void autonomousLeft();
void autonomousRight();

/*---------------------------------------------------------------------------*/
/*															  */
/*                          Pre-Autonomous Function                          */
/*															  */
/*---------------------------------------------------------------------------*/

void pre_auton()
{
	bStopTasksBetweenModes = true;

	LcdAutonomousSelection();
}

/*****************************************************************************/
/*															  */
/*                              Autonomous Section                           */
/*															  */
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
/*                            Pincer P Control Loop                          */
/*---------------------------------------------------------------------------*/
task pincerControlLoop()
{
	while (true){
		float error = SensorValue(pincerPos) - goToClawPos;
		float pGain;
		if(abs(error) >= pincerThres){
			pGain = kPPincer * error;
			movePincer(pGain);
		}
		else{
			movePincer(0);
		}
		wait1Msec(20);
	}
}

/*---------------------------------------------------------------------------*/
/*                              Arm PD Control Loop                          */
/*---------------------------------------------------------------------------*/

task armControlLoop()
{
	while (true){
		float error = SensorValue(armPos) - goToArmPos;
		float pGain;
		float lastError;
		float dGain;
		float deltaError;
		if(abs(error) >= armThres){
			pGain = kPArm * error;
			deltaError = error-lastError;
			dGain = deltaError*kDArm;
			moveArm(pGain + dGain);
			lastError = error;
		}
		else{
			moveArm(0);
			lastError = 0;
		}
		wait1Msec(20);
	}
}

/*---------------------------------------------------------------------------*/
/*                             Drive Straight Task                           */
/*---------------------------------------------------------------------------*/

task driveStraight()
{
	float distance = driveStraightDistance;
	resetMotorEncoder(driveBL);
	resetMotorEncoder(driveBR);
	bool atTarget = false;
	float kP = 0.8;
	float kD = 0;
	float kI = 0;
	float lastError = 0;
	float totalError = 0;
	float derivative = 0;
	while(atTarget == false){
		float avgDriveTicks = (getMotorEncoder(driveBR))+ (getMotorEncoder(driveBL))/2;
		float setTicks = (distance/(4*PI))*ticksPerRevDFWD;
		float error = setTicks - avgDriveTicks;
		totalError += error;
		derivative = error - lastError;
		float pGain = error      * kP;
		float iGain = totalError * kI;
		float dGain = derivative * kD;
		driveLeftSide(pGain + iGain + dGain);
		driveRightSide(pGain + iGain + dGain);
		lastError = error;
		if(abs(error) < 10){
			atTarget = true;
			driveStop();
		}
		wait1Msec(20);
	}
	stopTask(driveStraight);
}

/*---------------------------------------------------------------------------*/
/*															  */
/*                              Autonomous Task                              */
/*															  */
/*---------------------------------------------------------------------------*/

task autonomous()
{
	if( vSide == kSideLeft )
		autonomousLeft();
	else
		autonomousRight();
}

/*****************************************************************************/
/*															  */
/*                              User Control Task                            */
/*															  */
/*****************************************************************************/

task usercontrol()
{
	while (true)
	{
		//Drive - Left Side
		if(abs(vexRT[Ch3]) > driveThres){
			motor[driveBL] = vexRT[Ch3];
			motor[driveFL] = vexRT[Ch3];
		}
		else{
			motor[driveBL] = 0;
			motor[driveFL] = 0;
		}
		//Drive - Right Side
		if(abs(vexRT[Ch2]) > driveThres){
			motor[driveBR] = vexRT[Ch2];
			motor[driveFR] = vexRT[Ch2];
		}
		else{
			motor[driveBR] = 0;
			motor[driveFR] = 0;
		}
		//Arm
		motor[liftTL] = ((vexRT[Btn5U] - vexRT[Btn5D])*127);
		motor[liftBL] = ((vexRT[Btn5U] - vexRT[Btn5D])*127);
		motor[liftTR] = ((vexRT[Btn5U] - vexRT[Btn5D])*127);
		motor[liftBR] = ((vexRT[Btn5U] - vexRT[Btn5D])*127);
		//Pincer
		motor[pincerLeft] = ((vexRT[Btn6U] - vexRT[Btn6D])*127);
		motor[pincerRight] = -((vexRT[Btn6U] - vexRT[Btn6D])*127);
	}
}

/*---------------------------------------------------------------------------*/
/*															  */
/*                                 Functions                                 */
/*															  */
/*---------------------------------------------------------------------------*/

void moveArm(float pwr){
	motor[liftTL] = pwr;
	motor[liftBL] = pwr;
	motor[liftTR] = pwr;
	motor[liftBR] = pwr;
}
void movePincer(float pwr){
	motor[pincerLeft] = pwr;
	motor[pincerRight] = -pwr;
}
void drive(float distance){
	driveStraightDistance = distance;
	startTask(driveStraight);
}
void driveLeftSide(float pwr){
	motor[driveBL] = pwr;
	motor[driveFL] = pwr;
}
void driveRightSide(float pwr){
	motor[driveBR] = pwr;
	motor[driveFR] = pwr;
}
void driveStop(){
	driveLeftSide(0);
	driveRightSide(0);
}


/*-----------------------------------------------------------------------------*/
/*    Left and Right Autonomous                                                */
/*-----------------------------------------------------------------------------*/

void
autonomousRight()
{
	if( vStartPosition == kStartHanging ) {
		switch( vAuton ) {
		case    0:
			// run some autonomous code

			//Starting off postitions
			goToClawPos = SensorValue(pincerPos);
			goToArmPos = SensorValue(armPos);
			//Starting controll loops for the Pincer and Arm.
			startTask(pincerControlLoop);
			startTask(armControlLoop);
			//***** Route - Knock off Stars from Fence *****
			drive(-48); //Drive backwards 48 inches (2 Tiles).  1 tile = 24 inches.
			wait1Msec(500);
			goToClawPos = pincerFlush; //Set claw position to knock off stars
			wait1Msec(500);
			goToArmPos = armPrimed; //Set arm position to height to knock off stars
			wait1Msec(3000);
			drive(-12); //Drive backwards 12 inches
			wait1Msec(1000);
			goToArmPos = 1300; //Move arm higher to lift stars off fence
			wait1Msec(500);
			drive(-6); //Drive 6 inches backwards
			goToClawPos = 2200; //Close the claw halfway
			wait1Msec(1000);
			drive(24); //Drive 24 inches forward
			while(1){};
			break;
		case    1:
			// run some other autonomous code
			break;
		default:
			break;
		}
	}
	else { // middle zone
		switch( vAuton ) {
		case    0:
			// run some autonomous code
			break;
		case    1:
			// run some other autonomous code
			break;
		default:
			break;
		}
	}
}

void autonomousLeft()
{
	if( vStartPosition == kStartHanging ) {
		switch( vAuton ) {
		case    0:
			// run some autonomous code

			//Starting off postitions
			goToClawPos = SensorValue(pincerPos);
			goToArmPos = SensorValue(armPos);
			//Starting controll loops for the Pincer and Arm.
			startTask(pincerControlLoop);
			startTask(armControlLoop);
			//***** Route - Knock off Stars from Fence *****
			drive(-48); //Drive backwards 48 inches (2 Tiles).  1 tile = 24 inches.
			wait1Msec(500);
			goToClawPos = pincerFlush; //Set claw position to knock off stars
			wait1Msec(500);
			goToArmPos = armPrimed; //Set arm position to height to knock off stars
			wait1Msec(3000);
			drive(-12); //Drive backwards 12 inches
			wait1Msec(1000);
			goToArmPos = 1300; //Move arm higher to lift stars off fence
			wait1Msec(500);
			drive(-6); //Drive 6 inches backwards
			goToClawPos = 2200; //Close the claw halfway
			wait1Msec(1000);
			drive(24); //Drive 24 inches forward
			while(1){};
			break;
		case    1:
			// run some other autonomous code
			break;
		default:
			break;
		}
	}
	else { // middle zone
		switch( vAuton ) {
		case    0:
			// run some autonomous code
			break;
		case    1:
			// run some other autonomous code
			break;
		default:
			break;
		}
	}
}

/*-----------------------------------------------------------------------------*/
/*    Display menus and selections                                             */
/*-----------------------------------------------------------------------------*/

void LcdAutonomousDisplay(vexLcdMenus menu)
{
	// Cleat the lcd
	clearLCDLine(0);
	clearLCDLine(1);

	// Display the selection arrows
	displayLCDString(1,  0, l_arr_str);
	displayLCDString(1, 13, r_arr_str);
	displayLCDString(1,  5, "CHANGE");

	// Show the autonomous names
	switch( menu ) {
	case    kMenuSide:
		if( vSide == kSideLeft )
			displayLCDString(0, 0, "Side - Left");
		else
			displayLCDString(0, 0, "Side - Right");
		break;
	case    kMenuStartpos:
		if( vStartPosition == kStartHanging )
			displayLCDString(0, 0, "Start - Hanging");
		else
			displayLCDString(0, 0, "Start - Middle");
		break;
	case    kMenuAutonSelect:
		switch( vAuton ) {
		case    0:
			displayLCDString(0, 0, "Default");
			break;
		case    1:
			displayLCDString(0, 0, "Special 1");
			break;
		default:
			char    str[20];
			sprintf(str,"Undefined %d", vAuton );
			displayLCDString(0, 0, str);
			break;
		}
		break;

	default:
		displayLCDString(0, 0, "Unknown");
		break;
	}
}

/*-----------------------------------------------------------------------------*/
/*  Rotate through a number of menus and use center button to select choices   */
/*-----------------------------------------------------------------------------*/

void LcdAutonomousSelection()
{
	TControllerButtons  button;
	vexLcdMenus  menu = 0;

	// Turn on backlight
	bLCDBacklight = true;

	// diaplay default choice
	LcdAutonomousDisplay(0);

	while( bIfiRobotDisabled )
	{
		// this function blocks until button is pressed
		button = getLcdButtons();

		// Display and select the autonomous routine
		if( ( button == kButtonLeft ) || ( button == kButtonRight ) ) {
			// previous choice
			if( button == kButtonLeft )
				if( --menu < kMenuStart ) menu = kMenuMax-1;
			// next choice
			if( button == kButtonRight )
				if( ++menu >= kMenuMax ) menu = kMenuStart;
		}

		// Select this choice for the menu
		if( button == kButtonCenter )
		{
			switch( menu ) {
			case    kMenuSide:
				// alliance color
				vSide = (vSide == kSideLeft) ? kSideRight : kSideLeft;
				break;
			case    kMenuStartpos:
				// start position
				vStartPosition = (vStartPosition == kStartHanging) ? kStartMiddle : kStartHanging;
				break;
			case    kMenuAutonSelect:
				// specific autonomous routine for this position
				if( ++vAuton == 3 )
					vAuton = 0;
				break;
			}
		}
		// redisplay
		LcdAutonomousDisplay(menu);

		// Don't hog the cpu !
		wait1Msec(10);
	}
}
