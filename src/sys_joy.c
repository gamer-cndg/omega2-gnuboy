

#include "rc.h"
#include "input.h"

/* omega2 sepcific */
#include <stdio.h>
#include <string.h>
#include "WiiClassic.h"
#include <unistd.h>

wiiclassic_status_t lastGamepadStatus;
wiiclassic_status_t newGamepadStatus;

rcvar_t joy_exports[] =
{
	RCV_END
};

static int joyInit = 0;

void joy_init()
{
	if(joyInit == 0) {
		printf("Doing joy_init.\n");

		bool initOkay = WiiClassic_Init();
		printf("Init done.\n");
		while(!initOkay) {
			printf("No WiiClassic controller detected via I2C. Rechecking in 1 second.\n");
			sleep(1);
			initOkay = WiiClassic_Init();
		}

		//WiiClassic_ReadStatus(&newGamepadStatus);
		//WiiClassic_PrintStatus(&newGamepadStatus);

		joyInit = 1;
	}
}

void joy_close()
{
	//nothing to do
}

#define MAP_ENTRIES 10

typedef struct  {
	uint8_t buttonNewValue[MAP_ENTRIES];
	uint8_t buttonOldValue[MAP_ENTRIES];
	int eventCode[MAP_ENTRIES];
	const char* debugButtonNames[MAP_ENTRIES];
} button_map_t;

static button_map_t buttonMap = {
		.buttonNewValue = { 0 },
		.buttonOldValue = { 0 },
		.eventCode = { K_JOY1, K_JOY0, K_ENTER, K_SPACE, K_DOWN, K_LEFT, K_RIGHT, K_UP, K_INS, K_DEL },
		.debugButtonNames = { "A", "B", "START", "SELECT", "DOWN","LEFT","RIGHT","UP", "ZL","ZR"}
};

static int ignoreCount = 0;

void ev_poll()
{
	if(ignoreCount > 0) { ignoreCount--; return; }

	//read new status
	bool readOkay = WiiClassic_ReadStatus(&newGamepadStatus);
	if(!readOkay) {
		printf("JOYPAD ERROR OCCURED!!\n");
		//ignore the next 10 readings if this happens.
		//otherwise the gamepad will return that all
		//buttons were suddenly pressed and released
		//==> misfire on all buttons
		ignoreCount = 1000;
		return;
	}
	//WiiClassic_PrintStatus(&newGamepadStatus);

	//quick hack: are none of the D-PAD buttons pressed but the left analog stick is?
	//then we can re-interpret this as D-PADp presses
	// 0.5 is the relaxed position.
	//Reading must be at least 0.6 to register as positive.
	//and less than 0.4 to register as negative.
	const float minValuePlus = 0.6f;
	const float minValueMinus = 0.4f;

	if(newGamepadStatus.dpadDown == 0 && newGamepadStatus.dpadLeft == 0
			&& newGamepadStatus.dpadRight == 0 && newGamepadStatus.dpadUp == 0) {

		float analogX = newGamepadStatus.analogLeftX;
		float analogY = newGamepadStatus.analogLeftY;

		if(analogX > minValuePlus) newGamepadStatus.dpadRight = 1;
		else if(analogX < minValueMinus) newGamepadStatus.dpadLeft = 1;

		if(analogY > minValuePlus) newGamepadStatus.dpadUp = 1;
		else if(analogY < minValueMinus) newGamepadStatus.dpadDown = 1;
	}

	//what are the differences?
	uint8_t newButtons[MAP_ENTRIES] = {
			newGamepadStatus.buttonA, newGamepadStatus.buttonB,
			newGamepadStatus.buttonStart, newGamepadStatus.buttonSelect,
			newGamepadStatus.dpadDown, newGamepadStatus.dpadLeft,
			newGamepadStatus.dpadRight, newGamepadStatus.dpadUp,
			newGamepadStatus.shoulderZL, newGamepadStatus.shoulderZR
	};

	uint8_t lastButtons[MAP_ENTRIES] = {
			lastGamepadStatus.buttonA, lastGamepadStatus.buttonB,
			lastGamepadStatus.buttonStart, lastGamepadStatus.buttonSelect,
			lastGamepadStatus.dpadDown, lastGamepadStatus.dpadLeft,
			lastGamepadStatus.dpadRight, lastGamepadStatus.dpadUp,
			lastGamepadStatus.shoulderZL, lastGamepadStatus.shoulderZR
	};

	event_t ev;
	for(int i=0; i < MAP_ENTRIES; i++) {
		if(newButtons[i] != lastButtons[i]) {
			//pressed or released?
			ev.type = newButtons[i] == 0 ? EV_RELEASE : EV_PRESS;
			ev.code = buttonMap.eventCode[i];
			ev_postevent(&ev);

			printf("Button %s %s.\n", buttonMap.debugButtonNames[i], (ev.type == EV_RELEASE ? "released" : "pressed"));
		}
	}

	//set new state to old state
	memcpy(&lastGamepadStatus, &newGamepadStatus, sizeof(wiiclassic_status_t));
}
