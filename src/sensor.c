#include "stm32f10x_gpio.h"
#include "stm32f10x.h"
#include "delay.h"

//用于检测高低电平
#define H                       1
#define L                       0

//用于定义DA CL引脚
#define PIN_SDA                 GPIO_Pin_7
#define PIN_SCL                 GPIO_Pin_6

//IO组
#define PIN_PORT                GPIOB

// #define SDA_OUT               Gpio_InitIOExt(PIN_PORT,PIN_SDA,GpioDirOut,TRUE, FALSE, TRUE, FALSE) //????????
// #define SDA_IN                Gpio_InitIOExt(PIN_PORT,PIN_SDA,GpioDirIn,TRUE, FALSE, FALSE, FALSE) //?????
#define SDA_H                   GPIO_SetBits(PIN_PORT,PIN_SDA)
#define SDA_L                   GPIO_ResetBits(PIN_PORT,PIN_SDA)
#define SCL_H                   GPIO_SetBits(PIN_PORT,PIN_SCL)
#define SCL_L                   GPIO_ResetBits(PIN_PORT,PIN_SCL)
#define SDA                     GPIO_ReadInputDataBit(PIN_PORT,PIN_SDA)

//定义AD与量程转换参数，以下为标定10% - 90%，量程0 - 2000 kPa为例
#define PMIN 0  //Zero range pressure for example 0Kpa
#define PMAX 2000.0  //Full Point Pressure Value, for example 2000Kpa
#define DMIN 1677722.0  //AD value corresponding to pressure zero, for example 10%AD=2^24*0.1
#define DMAX 15099493.5  //AD Value Corresponding to Full Pressure Range, for example 90%AD=2^24*0.9

GPIO_InitTypeDef SCL_Pin;
GPIO_InitTypeDef SDA_Pin;

static void SCL_OUT(void)
{
	// GPIO_InitTypeDef SCL_Pin;

	SCL_Pin.GPIO_Pin = PIN_SCL;
	SCL_Pin.GPIO_Speed = GPIO_Speed_50MHz;
	SCL_Pin.GPIO_Mode = GPIO_Mode_Out_PP;

	GPIO_Init(PIN_PORT, &SCL_Pin);
}

static void SDA_OUT(void)
{
	// GPIO_InitTypeDef SDA_Pin;

	SDA_Pin.GPIO_Pin = PIN_SDA;
	SDA_Pin.GPIO_Speed = GPIO_Speed_50MHz;
	SDA_Pin.GPIO_Mode = GPIO_Mode_Out_PP;

	GPIO_Init(PIN_PORT, &SDA_Pin);
}

static void SDA_IN(void)
{
	// GPIO_InitTypeDef SDA_Pin;

	SDA_Pin.GPIO_Pin = PIN_SDA;
	SDA_Pin.GPIO_Speed = GPIO_Speed_50MHz;
	SDA_Pin.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	
	GPIO_Init(PIN_PORT, &SDA_Pin);
}

static void Start()
{
	SDA_OUT();
	SCL_OUT();
	SDA_H;
	delay_us(2);
	SCL_H;
	delay_us(2);
	SDA_L;
	delay_us(2);
	SCL_L;
}

static void Stop()
{
	SDA_OUT();
	SDA_L;
	delay_us(2);
	SCL_H;
	delay_us(2);
	SDA_H;
}

static unsigned char CheckAsk()
{
	unsigned char ack;
	unsigned char i;
	SDA_H;
	SDA_IN();
	SCL_H;
	delay_us(2);
	for(i=0;i<3;i++)
	{
		if(SDA==0)
		{
			ack=0;
			break;
		}
		else ack=1;
	}
	SCL_L;
	SDA_OUT();
	return ack;
}

static void SendAsk()
{
  SDA_OUT();
  SDA_L;
  delay_us(2);
  SCL_H;
  delay_us(2);
  SCL_L;
  SDA_IN();
}

static void SendByte(unsigned char DATA)
{
	unsigned char i=0;
	do
	{
		if(DATA&0x80)
		{
			SDA_H;
		}
		else 
		{
			SDA_L;
		}
		SCL_H;
		delay_us(2);
		DATA<<=1;
		i++;
		SCL_L;
		delay_us(2);
	} while (i<8);
}

static void GetByte(unsigned char *DATA)
{
  unsigned char temp=0;
  unsigned char i;
  SDA_IN();
  for(i=0;i<8;i++)
  {
    SCL_H;
	delay_us(2);
    if(SDA)temp|=0x01;
    else temp&=0xFE;
    if(i<7)
    {
      temp<<=1;
    }
    SCL_L;
	delay_us(2);
  }
  *DATA=temp;
  SDA_OUT();
}

//写寄存器
unsigned char WriteSensor(void)
{
	Start();
	SendByte(0xF0);    
	if(CheckAsk())
	{
		Stop();
		return 1;
	}
    SendByte(0xAC);  
	if(CheckAsk())
	{
		Stop();
		return 2;
	}
	Stop();
	return 0;
}

//读寄存器
unsigned char ReadSensor(unsigned char RedAdd,unsigned char *DATA,unsigned char count)
{
	Start();
	SendByte(0xF1);    
	if(CheckAsk())
	{
		Stop();
		return 1;
	}
	while(count)
	{
		GetByte(DATA);
    if(count>1)SendAsk();
		*DATA--;
		count--;
	}
	Stop();
	return 0;
}

//压力读取读取
float Get_Pressure_DATA(void)
{
	typedef union
	{
		unsigned char Byte[4];
		unsigned long u32;
		long i32;
	} user_32Bit;
	
	float pressure = 0.0;
	unsigned char Error_code1 = 0;  //Debug
	unsigned char Error_code2 = 0;  //Debug
	user_32Bit AD_DATA;
	AD_DATA.u32 = 0;

	//Send 0xAC command and read the returned six-byte data
    Error_code1 = WriteSensor();
	delay_ms(50);
	Error_code2 = ReadSensor(0x00,&AD_DATA.Byte[3],4);

	AD_DATA.i32 = (AD_DATA.i32 << 8) / 256;

	pressure = (PMAX-PMIN)/(DMAX-DMIN)*(AD_DATA.i32-DMIN)+PMIN;

	return pressure;
}
