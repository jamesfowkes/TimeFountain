#include <SerialMessaging/SerialMessaging.h>

#define ADC_PIN A0
#define MOTOR_PIN 0

static void SerialMessageCallback(String* message);

static SerialMessaging s_SerialMessaging(SerialMessageCallback);

static uint8_t currentPWM = 0;
static uint8_t lowerLimit = 0;
static uint8_t upperLimit = 0;

void setup()
{
	s_SerialMessaging.Begin(115200);
}

void loop()
{
	readMotorControlPot();
	updatePWMOutput();
	if (serialEventRun) { serialEventRun(); }
}

static void SerialMessageCallback(String* message)
{
	if ( message.startsWith("MAX") )
	{
		upperLimit = (uint8_t)atoi(bytes + 3);
		
		if (upperLimit < lowerLimit)
		{
			lowerLimit = upperLimit;
		}
	}
	
	if ( message.startsWith("MIN") )
	{
		lowerLimit = (uint8_t)atoi(bytes + 3);
		if (lowerLimit > upperLimit)
		{
			upperLimit = lowerLimit;
		}
	}
}

void readMotorControlPot(void)
{
	uint16_t rawADC = analogRead(ADC_PIN);
	currentPWM = map(rawADC, 0, 1023, lowerLimit, upperLimit);
}

void updatePWMOutput(void)
{
	analogWrite(MOTOR_PIN, currentPWM);
}

void serialEvent()
{
	s_SerialMessaging.SerialEvent();
}
