

void OnGpioButton_Down(size_t pin);
void OnGpioButton_Up(size_t pin);
void OnGpioButton_LongPress(size_t pin);

#define GPIO_MAX_NUM					2
#define GPIO_CHATTERING_IGNORE		  50 		// msec
#define GPIO_LONG_PRESS 			1000	  // msec

#include <stdint.h>
#include <Arduino.h>
#include <TimerTC3.h>



typedef void (*GPIO_TIMER_FUNC)(void);
void GpioTimerIsr();

class GpioTimer
{
public:
	GpioTimer()
	{
		TimerTc3.initialize(5000); // micro sec
		TimerTc3.attachInterrupt(GpioTimerIsr);
	}

	void SetTimer(GPIO_TIMER_FUNC func, int32_t timeout)
	{
		for (size_t i = 0; i < GPIO_MAX_NUM; i++)
		{
			if (m_tTimer[i].func == NULL)
			{
				m_tTimer[i].timeout = millis() + timeout;
				m_tTimer[i].func = func;
			
				TimerTc3.start();
//				SERIAL.println("SetTimer - Success");
				return;
			}
		}

//		SERIAL.println("SetTimer - MAX CH");
	}

	void KillTimer(GPIO_TIMER_FUNC func)
	{
		for (size_t i = 0; i < GPIO_MAX_NUM; i++)
		{
			if (m_tTimer[i].func == func)
			{
				m_tTimer[i].func = NULL;
			}
		}
	}

	void OnTimerIsr()
	{
		bool isStopTimer = true;
		uint32_t current = millis();
		for (size_t i = 0; i < GPIO_MAX_NUM; i++)
		{
			if (m_tTimer[i].func != NULL)
			{
				if ((int32_t)(m_tTimer[i].timeout - current) < 0)
				{
					GPIO_TIMER_FUNC func = m_tTimer[i].func;
					m_tTimer[i].func = NULL;

					func();
				}
			}
		}

		for (size_t i = 0; i < GPIO_MAX_NUM; i++)
		{
			if (m_tTimer[i].func != NULL)
			{
				isStopTimer	= false;
				break;
			}
		}

		if (isStopTimer)
		{
			TimerTc3.stop();
		}
	}

protected:
	struct
	{
		GPIO_TIMER_FUNC func;
		uint32_t timeout;
	} m_tTimer[GPIO_MAX_NUM];
};

GpioTimer g_iTimer;
void GpioTimerIsr()
{
	g_iTimer.OnTimerIsr();
};

enum GPIO_STATE
{
	GPIO_STATE_UP = 0,
	GPIO_STATE_WAIT_DOWN_STABLE,
	GPIO_STATE_DOWN,
	GPIO_STATE_WAIT_UP_STABLE,
};

void	dummyGpioFunc(){};

#define GPIO_BUTTON_FUNC_START(GPIO_PIN)												\
attachInterrupt(GPIO_PIN, GpioButtonFunc##GPIO_PIN, RISING)

#define GPIO_BUTTON_FUNC_DECLARE(GPIO_PIN)												\
uint8_t g_iGpioState##GPIO_PIN = GPIO_STATE_UP;											\
void GpioButton_LongPress##GPIO_PIN(void)												\
{																						\
	OnGpioButton_LongPress(GPIO_PIN);													\
	g_iTimer.SetTimer( GpioButton_LongPress##GPIO_PIN, GPIO_LONG_PRESS );				\
}																						\
																						\
void GpioButtonFunc##GPIO_PIN(void)														\
{																						\
	switch (g_iGpioState##GPIO_PIN)														\
	{																					\
	case GPIO_STATE_UP:																	\
		attachInterrupt(GPIO_PIN,dummyGpioFunc,CHANGE);									\
																						\
		OnGpioButton_Down(GPIO_PIN);													\
																						\
		g_iGpioState##GPIO_PIN = GPIO_STATE_WAIT_DOWN_STABLE;							\
		g_iTimer.SetTimer( GpioButtonFunc##GPIO_PIN, GPIO_CHATTERING_IGNORE );			\
		break;																			\
																						\
	case GPIO_STATE_WAIT_DOWN_STABLE:													\
		if (digitalRead(GPIO_PIN))														\
		{																				\
			g_iGpioState##GPIO_PIN = GPIO_STATE_DOWN;									\
			attachInterrupt(GPIO_PIN, GpioButtonFunc##GPIO_PIN, FALLING);				\
																						\
			/* for LongPress detection */												\
			g_iTimer.SetTimer( GpioButton_LongPress##GPIO_PIN, GPIO_LONG_PRESS );		\
		}																				\
		else																			\
		{																				\
			OnGpioButton_Up(GPIO_PIN);													\
																						\
			g_iGpioState##GPIO_PIN = GPIO_STATE_UP;										\
			attachInterrupt(GPIO_PIN, GpioButtonFunc##GPIO_PIN, RISING);				\
		}																				\
		break;																			\
																						\
	case GPIO_STATE_DOWN:																\
		attachInterrupt(GPIO_PIN,dummyGpioFunc,CHANGE);									\
		g_iTimer.KillTimer( GpioButton_LongPress##GPIO_PIN );							\
																						\
		OnGpioButton_Up(GPIO_PIN);														\
																						\
		g_iGpioState##GPIO_PIN = GPIO_STATE_WAIT_UP_STABLE;								\
		g_iTimer.SetTimer( GpioButtonFunc##GPIO_PIN, GPIO_CHATTERING_IGNORE );			\
		break;																			\
																						\
	case GPIO_STATE_WAIT_UP_STABLE:														\
		if (!digitalRead(GPIO_PIN))														\
		{																				\
			g_iGpioState##GPIO_PIN = GPIO_STATE_UP;										\
			attachInterrupt(GPIO_PIN, GpioButtonFunc##GPIO_PIN, RISING);				\
		}																				\
		else																			\
		{																				\
			OnGpioButton_Down(GPIO_PIN);												\
																						\
			g_iGpioState##GPIO_PIN = GPIO_STATE_DOWN;									\
			attachInterrupt(GPIO_PIN, GpioButtonFunc##GPIO_PIN, FALLING);				\
		}																				\
		break;																			\
	}																					\
}



