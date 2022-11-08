



#include "main.h"

//
//typedef struct {
//	GPIO_TypeDef * port;
//	uint32_t pin;
//} PinPort;

typedef struct {
	GPIO_TypeDef * portA;
	uint32_t pinA;
	GPIO_TypeDef * portB;
	uint32_t pinB;

} PinPair;


//table for doing charlieplexing. each record has a unique pairing of pins
//take the LED number, shift off the LSB, and look it up (20 LEDs = 10 table entries)
//then use the LSB to determine the direction between the pair

PinPair pinPairs[] = {
		{SB_A0_GPIO_Port, SB_A0_Pin, SB_A1_GPIO_Port, SB_A1_Pin},
		{SB_A0_GPIO_Port, SB_A0_Pin, SB_A2_GPIO_Port, SB_A2_Pin},
		{SB_A0_GPIO_Port, SB_A0_Pin, SB_A3_GPIO_Port, SB_A3_Pin},
		{SB_A0_GPIO_Port, SB_A0_Pin, SB_A4_GPIO_Port, SB_A4_Pin},

		{SB_A1_GPIO_Port, SB_A1_Pin, SB_A2_GPIO_Port, SB_A2_Pin},
		{SB_A1_GPIO_Port, SB_A1_Pin, SB_A3_GPIO_Port, SB_A3_Pin},
		{SB_A1_GPIO_Port, SB_A1_Pin, SB_A4_GPIO_Port, SB_A4_Pin},

		{SB_A2_GPIO_Port, SB_A2_Pin, SB_A3_GPIO_Port, SB_A3_Pin},
		{SB_A2_GPIO_Port, SB_A2_Pin, SB_A4_GPIO_Port, SB_A4_Pin},

		{SB_A3_GPIO_Port, SB_A3_Pin, SB_A4_GPIO_Port, SB_A4_Pin},
};


void setLed(int ledNum) {
	//TODO maybe more efficient to write to MODER on A and B directly

	//set all to input

	LL_GPIO_SetPinMode(SB_A0_GPIO_Port, SB_A0_Pin, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinMode(SB_A1_GPIO_Port, SB_A1_Pin, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinMode(SB_A2_GPIO_Port, SB_A2_Pin, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinMode(SB_A3_GPIO_Port, SB_A3_Pin, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinMode(SB_A4_GPIO_Port, SB_A4_Pin, LL_GPIO_MODE_INPUT);


	int dir = ledNum & 1;
	PinPair * pair = &pinPairs[ledNum >> 1];

	if (dir) {
		LL_GPIO_ResetOutputPin(pair->portA, pair->pinA);
		LL_GPIO_SetOutputPin(pair->portB, pair->pinB);
	} else {
		LL_GPIO_ResetOutputPin(pair->portB, pair->pinB);
		LL_GPIO_SetOutputPin(pair->portA, pair->pinA);
	}

	LL_GPIO_SetPinMode(pair->portA,  pair->pinA, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinMode(pair->portB,  pair->pinB, LL_GPIO_MODE_OUTPUT);

}
