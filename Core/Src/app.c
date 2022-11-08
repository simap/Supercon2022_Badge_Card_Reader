
#include "main.h"
#include "string.h"

static const int PAGE_TIMEOUT_MS = 5000;
static const int LOW_THRESHOLD = 2100;
static const int HIGH_THRESHOLD = 2200;


void setLed(int ledNum);


volatile uint32_t ms = 0; //updated by SysTick

volatile uint16_t adcBuffer[13]; //buffer for DMA
volatile uint16_t currentRow[13]; //working copy/snapshot

int scanNumber = 0;
volatile int rowReady;


typedef enum {
	WAIT_FOR_PAGE, WAIT_FOR_SYNC, IN_SYNC, PAGE_ERROR
} SyncMode;

SyncMode syncMode = WAIT_FOR_PAGE;

//current program page

uint16_t page[32];
int currentWordIndex;

//for timeout, error checking purposes
uint32_t scanStartTimer;
int totalScanWordCount = 0;

uint16_t program[32 * 4];
int programPageCount;


void initAdc() {

	LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_55CYCLES_5);

	LL_ADC_Enable(ADC1);
}

void initTim1() {

	TIM1->PSC = 0;

	TIM1->ARR = 4799; //48mhz / 4800 = 10khz

	TIM1->CR1 |= TIM_CR1_CEN | TIM_CR1_ARPE; // enable and auto-reload (why?)
	TIM1->CR1 |= TIM_CR1_CEN ; // enable

	LL_TIM_EnableIT_UPDATE(TIM1);
}

void initDma() {
	//configure DMA to read from ADC into a buffer
	DMA1_Channel1->CPAR = (uint32_t) (&(ADC1->DR)); //point dma to ADC data reg
	DMA1_Channel1->CMAR = (uint32_t) (adcBuffer); //point DMA to buffer memory
	DMA1_Channel1->CNDTR = 13; //count of transfers per circle
	DMA1_Channel1->CCR |= DMA_CCR_EN; //enable channel 1 dma

}


void delay(int t) {
	uint32_t timer = ms;
	while (ms - timer < t) {
		//wait
	}
}



/*
 * Each time the timer fires bounce back and forth between changing the LED, and starting an ADC sample
 */
void nextSequence() {
	static int toggle = 0;

	if (toggle) {
		setLed(scanNumber);
		toggle = 0;
		scanNumber++;
		if (scanNumber >= 13) {
			scanNumber = 0;
			memcpy(currentRow, adcBuffer, sizeof(currentRow));
			rowReady = 1;
		}
	} else {
		ADC1->CR |= ADC_CR_ADSTART;
		toggle = 1;
	}
}


void resetState() {
	syncMode = WAIT_FOR_PAGE;
	memset(program, 0, sizeof(program));
	programPageCount = 0;

	memset(page, 0, sizeof(page));
	currentWordIndex = 0;

	scanStartTimer = ms;
	totalScanWordCount = 0;
}

void sendCh(uint8_t c) {
	LL_USART_TransmitData8(USART1, c);

	while (!LL_USART_IsActiveFlag_TXE(USART1))
		;//wait
}


void sendWord(uint16_t w) {
	LL_USART_TransmitData8(USART1, w & 0xff);
	while (!LL_USART_IsActiveFlag_TXE(USART1))
		;//wait

	LL_USART_TransmitData8(USART1, w>>8);
	while (!LL_USART_IsActiveFlag_TXE(USART1))
		;//wait

}

void sendProgram() {

	//TODO do we need to trim off trailing zero instructions? the badge does. Only counts the last nonzero one.
	uint16_t wordCount = 0;
	uint16_t checksum = 0;

	for (int i = 0; i < programPageCount * 32; i++) {
		if (program[i]) {
			wordCount = i+1;
		}
	}


	/*
	1. Header 6 bytes: 00 FF 00 FF A5 C3
	2. Program length 2 bytes (in 16-bit words, Low byte first): NN NN
	3. Program NN 0N×Program Length (Low first): NN 0N, NN 0N, NN 0N...
	4. 16-bit Checksum 2 bytes (items 2 and 3 only, Low first): NN NN
	 */
	sendCh(0x00);
	sendCh(0xFF);
	sendCh(0x00);
	sendCh(0xFF);
	sendCh(0xA5);
	sendCh(0xC3);

	sendWord(wordCount);
	checksum += wordCount;

	for (int i = 0; i < wordCount; i++) {
		sendWord(program[i]);
		checksum += program[i];
	}

	sendWord(checksum);

}

void errorFlash() {
	uint32_t timer = ms;
	while (ms - timer < 1000) {
		LL_GPIO_ResetOutputPin(LED_GPIO_Port, LED_Pin);
		if (ms & 0x100) {
//			LL_GPIO_SetOutputPin(LED_GPIO_Port, LED_Pin);
		}
	}
}


void run() {


	DBGMCU->APB1FZ = 0xffffffff;
	DBGMCU->APB2FZ = 0xffffffff;

	setLed(scanNumber);
	scanNumber++; //next scan should be 1

	initAdc();
	initDma();
	initTim1();
	LL_USART_Enable(USART1);

	LL_SYSTICK_EnableIT();

	for(;;) {

		//test one LED

//		volatile int tled = 5;
//		for(;;) {
//			setLed(tled);
//
//
//			delay(100);
//
//		}

		//test scanning across LEDs
//		for (int i = 0; i < 13; i++) {
//			setLed(i);
//			delay(100);
//		}
//
//		continue;


		//check for a complete row
		if (rowReady) {
			rowReady = 0;

			LL_GPIO_ResetOutputPin(LED_GPIO_Port, LED_Pin);


			switch (syncMode) {

			case PAGE_ERROR:

				if (ms & 0x200) {
					LL_GPIO_SetOutputPin(LED_GPIO_Port, LED_Pin);
				}

				//fall through
			case WAIT_FOR_PAGE:
				scanStartTimer = ms;
				totalScanWordCount = 0;
				currentWordIndex = 0;

				if (!LL_GPIO_IsInputPinSet(BTN_GPIO_Port, BTN_Pin)) {
					if (programPageCount > 0) {
						//transmit program!!!!
						LL_GPIO_SetOutputPin(LED_GPIO_Port, LED_Pin);
						sendProgram();
						LL_GPIO_ResetOutputPin(LED_GPIO_Port, LED_Pin);
					}

					resetState();
				}

				//fall through
			case WAIT_FOR_SYNC:

				//is the sync bit going low?
				if (currentRow[0] < LOW_THRESHOLD) {
					syncMode = IN_SYNC;
					scanStartTimer = ms;
					//clear this word
					page[currentWordIndex] = 0;
				} else if (ms - scanStartTimer > PAGE_TIMEOUT_MS) {
					if (totalScanWordCount != 32) {
						syncMode = PAGE_ERROR;
						errorFlash();
						break;
					} else {
						syncMode = WAIT_FOR_PAGE;
						//page full!!!
						//TODO reverse and buffer this in the larger program storage
						for (int i = 0; i < 32; i++) {
							program[i + 32 * programPageCount] = page[31 - i];
						}
						programPageCount++;
					}
				}

				break;
			case IN_SYNC:

				//is sync bit going high?
				if (currentRow[0] > HIGH_THRESHOLD) {
					syncMode = WAIT_FOR_SYNC;
					scanStartTimer = ms;
					currentWordIndex++;

					totalScanWordCount++;
					if (totalScanWordCount > 32) {
						syncMode = PAGE_ERROR;
						errorFlash();
					}

					if (currentWordIndex >= 32) {
						currentWordIndex = 0;
					}
					break;
				} else if (ms - scanStartTimer > PAGE_TIMEOUT_MS) {
					syncMode = PAGE_ERROR;
					errorFlash();
					break;
				}

				//parse the word, checking the current row against low threshold
				uint16_t word = 0;
				for (int i = 0; i < 12; i++) {
					volatile uint16_t bitReading = currentRow[i+1];
					if (bitReading < LOW_THRESHOLD) {
						word |= 1<<i;
					}
				}

				//set any bits in this word, preserving previous bits (make them sticky during sync)
				page[currentWordIndex] |= word;

				break;

			}

		}
	}
}

