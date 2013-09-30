/*
 * Standard Library Includes
 */

#include <stdbool.h>

/*
 * AVR Includes (Defines and Primitives)
 */
#include <avr/io.h>
#include <avr/wdt.h>

/*
 * AVR Library Includes
 */

#include "lib_clk.h"
#include "lib_io.h"
#include "lib_adc.h"
#include "lib_pcint.h"
#include "lib_tmr_common.h"
#include "lib_tmr16.h"
#include "lib_tmr8_tick.h"

/*
 * Private Defines and Datatypes
 */

#define APPLICATION_TICK_MS (20)
#define HEARTBEAT_TICK_MS (500)

#define ONE_MILLISECOND_US (1000U)

#define STROBE_PERIOD_MS (49U)
#define STROBE_PERIOD_US (STROBE_PERIOD_MS * ONE_MILLISECOND_US)

#define NO_DROP_MOTION_SPEED_COUNTS (0x1FF)

#define MAX_STROBES (10)

#define ADC_DIRECTION_CH LIB_ADC_CH_0
#define ADC_POSITION_CH LIB_ADC_CH_1

#define HEARTBEAT_PORT PIND
#define HEARTBEAT_PIN 0

#define MOTOR_PORT IO_PORTB
#define MOTOR_PIN 0

#define DIRECTION_PORT IO_PORTB
#define DIRECTION_PIN 1

#define POSITION_PORT IO_PORTB
#define POSITION_PIN 2

#define STROBE_PORT PORTB
#define STROBE_PIN 2

/*
 * Function Prototypes
 */
 
static void setupIO(void);
static void setupADC(void);
static void setupTimer(void);

static void adcHandler(void);

static void setMotorSpeed(uint16_t adcReading);
static void setStrobeDelay(uint16_t adcReading);

static void strobeOn(void);
static void strobeWaitCount(void);
static void strobeOff(void);

static void applicationTick(void);

/*
 * Private Variables
 */
 
/* Library control structures */
static TMR8_TICK_CONFIG appTick;
static TMR8_TICK_CONFIG hbTick;
static TIMER_FLAG strobeOnFlag;
static TIMER_FLAG strobeOffFlag;
static TIMER_FLAG strobeWaitFlag;

static ADC_CONTROL_ENUM adc;

static PCINT_FLAG pc;

static uint16_t s_strobeWaitMs = 0;
static uint32_t s_strobeDelayUs = 0;
static uint8_t s_strobeCount = 0;

int main(void)
{

	/* Disable watchdog: not required for this application */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	
	setupIO();
	setupADC();
	setupTimer();
	
	while (true)
	{
		bool strobeProcessing = false;
		
		do
		{
			if (PCINT_TestAndClear(&pc))
			{
				TMR16_StopTimer();
				
				s_strobeWaitMs = STROBE_PERIOD_MS;
				s_strobeCount = 0;
				
				if (s_strobeDelayUs)
				{
					TMR16_StartTimer(s_strobeDelayUs, &strobeOnFlag, TMR_OCCHAN_B);
				}
				else
				{
					strobeOnFlag = true;
				}
				strobeProcessing = true;
			}
			
			if (TMR_TestAndClear(&strobeOnFlag))
			{
				strobeOn();
				strobeProcessing = true;
			}
			
			if (TMR_TestAndClear(&strobeWaitFlag))
			{
				strobeWaitCount();
				strobeProcessing = true;
			}
			
			if (TMR_TestAndClear(&strobeOffFlag))
			{
				strobeOff();
				strobeProcessing = true;
			}
		
		} while (strobeProcessing);
		
		if (ADC_TestAndClear(&adc))
		{
			adcHandler();
		}
		
		if (TMR8_Tick_TestAndClear(&appTick))
		{
			applicationTick();
		}
		
		if (TMR8_Tick_TestAndClear(&hbTick))
		{
			IO_Toggle(HEARTBEAT_PORT, HEARTBEAT_PIN);
		}
	}
}

/*
 * Private Functions
 */

static void setupIO(void)
{
	IO_SetMode(MOTOR_PORT, MOTOR_PIN, IO_MODE_OUTPUT);
	IO_SetMode(DIRECTION_PORT, DIRECTION_PIN, IO_MODE_INPUT);
	IO_SetMode(POSITION_PORT, POSITION_PIN, IO_MODE_INPUT);
}

static void setupADC(void)
{
	ADC_SelectPrescaler(LIB_ADC_PRESCALER_DIV64);
	ADC_SelectReference(LIB_ADC_REF_VCC);
	ADC_Enable(true);
	ADC_EnableInterrupts(true);

	adc.busy = false;
	adc.channel = ADC_DIRECTION_CH;
	adc.conversionComplete = false;
}

static void setupTimer(void)
{
	CLK_Init(0);
	TMR8_Tick_Init();

	appTick.reload = APPLICATION_TICK_MS;
	appTick.active = true;
	TMR8_Tick_AddTimerConfig(&appTick);
	
	hbTick.reload = HEARTBEAT_TICK_MS;
	hbTick.active = true;
	TMR8_Tick_AddTimerConfig(&hbTick);

	TMR16_SetSource(TMR_SRC_FCLK);
	TMR16_SetCountMode(TMR16_COUNTMODE_FASTPWM_10BIT);
}

static void applicationTick(void)
{
	if (!adc.busy)
	{
		adc.channel = (adc.channel == ADC_DIRECTION_CH) ? ADC_POSITION_CH : ADC_DIRECTION_CH;
		ADC_GetReading(&adc);
	}
}

static void adcHandler(void)
{
	switch (adc.channel)
	{
	case ADC_DIRECTION_CH:
		setMotorSpeed(adc.reading);
		break;
	case ADC_POSITION_CH:
		setStrobeDelay(adc.reading);
		break;
	default:
		break;
	}
}

static void setStrobeDelay(uint16_t adcReading)
{
	// Maximum ADC reading corresponds to a delay of the strobe period
	s_strobeDelayUs = (adcReading * STROBE_PERIOD_US) / MAX_10BIT_ADC;
	
	if (s_strobeDelayUs < 10)
	{
		s_strobeDelayUs = 0; // Deadband near zero
	}
}

static void setMotorSpeed(uint16_t adcReading)
{
	int16_t scaledReading = (int16_t)adcReading; // ADC reading is only 10-bit - no loss
	scaledReading -= NO_DROP_MOTION_SPEED_COUNTS; // Shift so that center position is default "no motion" speed
	scaledReading >>= 3; // Do not allow motor to go too fast or too slow
	
	if ((scaledReading < 10) && (scaledReading > -10))
	{
		scaledReading = 0; // Deadband around zero
	}
	
	TMR16_SetOutputCompareValue(NO_DROP_MOTION_SPEED_COUNTS + scaledReading, TMR_OCCHAN_A);
}

static void strobeOn(void)
{
	IO_On(STROBE_PORT, STROBE_PIN);
	TMR16_StartTimer(ONE_MILLISECOND_US, &strobeOffFlag, TMR_OCCHAN_B);
}

static void strobeWaitCount(void)
{
	if (s_strobeWaitMs-- == 0)
	{
		strobeOn();
		TMR16_StartTimer(ONE_MILLISECOND_US, &strobeOffFlag, TMR_OCCHAN_B);
	}
	else
	{
		TMR16_StartTimer(ONE_MILLISECOND_US, &strobeWaitFlag, TMR_OCCHAN_B);
	}
}

static void strobeOff(void)
{
	IO_Off(STROBE_PORT, STROBE_PIN);
	s_strobeWaitMs = STROBE_PERIOD_MS;
	
	if (++s_strobeCount < MAX_STROBES)
	{
		TMR16_StartTimer(ONE_MILLISECOND_US, &strobeWaitFlag, TMR_OCCHAN_B);
	}
}
