/*#############################################################
Driver name	    : hc06.c
Author 			: Grant Phillips
Date Modified   : 25/09/2014
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)
Tested On       : STM32F4-Discovery

Description			: Provides a library to access the USART module
					  on the STM32F4-Discovery to establish serial
					  communication with a remote device (e.g. PC)
					  using the HC-06 Bluetooth module.

Requirements    : * STM32F4-Discovery Board
				  * HC-06 Bluetooth module

Functions		: HC06_Init
				  HC06_PutChar
				  HC06_PutStr
				  HC06_ClearRxBuffer
				  HC06_Test
				  HC06_SetBaud
				  HC06_SetName
				  HC06_SetPin

Special Note(s) : In this driver PC6 is used as USART_TX and
					PC7 as USART_RX. Any other UART and pins can
					be used, just change the relevant GPIO config-
					urations and occurrences of USART to the new
					UART/USART number.
##############################################################*/

#include <string.h>
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_gpio.h"
#include "misc.h"
#include "stm32f4xx.h"
#include "hc06.h"

/* Macros for controlling 'nth' USART peripheral - BEGIN */
#define USART USART6
#define USART_IRQn USART6_IRQn
#define GPIO_AF_USART GPIO_AF_USART6
#define RCC_APB2Periph_USART RCC_APB2Periph_USART6

#define TX_PIN GPIO_Pin_6
#define RX_PIN GPIO_Pin_7
#define TX_SOURCE GPIO_PinSource6
#define RX_SOURCE GPIO_PinSource7

#define USART_PORT GPIOC
#define RCC_AHB1Periph_GPIO RCC_AHB1Periph_GPIOC
/* Macros for controlling 'nth' USART peripheral - END */

#define HC06_RX_BUFFER_LENGTH 40 //maximum number of characters to hold in the receive buffer
#define	HC06_TIMEOUT_MAX 94000 //timeout value for waiting for a response from the HC-06 (+/- 2 secs)

char HC06_rx_buffer[HC06_RX_BUFFER_LENGTH];	//used by the IRQ handler
uint8_t HC06_rx_counter = 0; //used by the IRQ handler
char HC06_msg[HC06_RX_BUFFER_LENGTH]; //variable that contains the latest string received on the RX pin
uint8_t new_HC06_msg = 0; //flag variable to indicate if there is a new message to be serviced

/*********************************************************************************************
Function name   : HC06_Delay
Author 			: Grant Phillips
Date Modified   : 06/08/2013
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Creates a delay using a for loop.

Special Note(s) : NONE

Parameters		: delay - delay value in 100us increments, i.e. delay = 1 means 100us delay

Return value	: NONE
*********************************************************************************************/
inline void HC06_Delay(uint32_t delay)
{
	for(; delay; --delay);
}

/*********************************************************************************************
Function name   : HC06_Init
Author 			: Andrei Istodorescu
Date Modified   : 02/08/2014 (Grant Phillips)
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Initializes the HC-06 Bluetooth module

Special Note(s) : NONE

Parameters		: speed	- 32-bit value to set the baud rate
Return value	: NONE
*********************************************************************************************/
void HC06_Init(uint32_t speed)
{
	USART_InitTypeDef USART_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	/* Enable GPIO clock */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIO, ENABLE);
	/* Enable USART clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART, ENABLE);

	/* Configure USART Tx and Rx as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin = TX_PIN | RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(USART_PORT, &GPIO_InitStructure);
	/* Connect PC6 to USART_Tx */
	GPIO_PinAFConfig(USART_PORT, TX_SOURCE, GPIO_AF_USART);
	/* Connect PC7 to USART_Rx */
	GPIO_PinAFConfig(USART_PORT, RX_SOURCE, GPIO_AF_USART);

	/* USART configuration */
	/* USARTx configured as follow:
		- BaudRate = speed parameter above
		- Word Length = 8 Bits
		- One Stop Bit
		- No parity
		- Hardware flow control disabled (RTS and CTS signals)
		- Receive and transmit enabled
	*/
	USART_InitStructure.USART_BaudRate = speed;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART, &USART_InitStructure);
	/* Enable the UART6 Receive interrupt: this interrupt is generated when the
		USART6 receive data register is not empty */
	USART_ITConfig(USART, USART_IT_RXNE, ENABLE);

	/* Enable the USART Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USART_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	/* Enable USART */
	USART_Cmd(USART, ENABLE);

	/* Flush USART Tx Buffer */
	HC06_PutStr("\n\r");
	HC06_ClearRxBuffer();
	HC06_Delay(20000000);
}

/*********************************************************************************************
Function name   : UART6_IRQHandler
Author 			: Andrei Istodorescu
Date Modified   : 05/08/2014 (Grant Phillips)
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Handles the interrupt when a new byte is received on the RX pin.  It works
					on the assumption that the incoming message will be terminated by a
					LF (0x10) character.

Special Note(s) : NONE

Parameters		: NONE
Return value	: NONE
*********************************************************************************************/
#define DEBUG 1

void USART6_IRQHandler(void)
{	
	if(USART_GetITStatus(USART, USART_IT_RXNE) != RESET)
	{
		/* Read one byte from the receive data register */
		HC06_rx_buffer[HC06_rx_counter] = USART_ReceiveData(USART);
#if DEBUG==1
		if(HC06_rx_buffer[HC06_rx_counter]=='\r')
			HC06_PutChar('\n');
		HC06_PutChar(HC06_rx_buffer[HC06_rx_counter]); /* echo back (debug only) */
#endif

		/* if the last character received is the LF ('\r' or 0x0a) character OR if the HC06_RX_BUFFER_LENGTH (40) value has been reached ...*/
		if((HC06_rx_counter + 1 == HC06_RX_BUFFER_LENGTH) || (HC06_rx_buffer[HC06_rx_counter] == 0x0a)) {
		  memcpy(HC06_msg, HC06_rx_buffer, HC06_rx_counter); //copy each character in the HC06_rx_buffer to the HC06_msg variable
		  memset(HC06_rx_buffer, 0, HC06_RX_BUFFER_LENGTH); //clear HC06_rx_buffer
		  HC06_rx_counter = 0;
		  new_HC06_msg = 1;
		} else {
			HC06_rx_counter++;
		}
	}
}

/*********************************************************************************************
Function name   : HC06_PutChar
Author 			: Grant Phillips
Date Modified   : 05/08/2013
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Writes a character to the HC-06 Bluetooth module.

Special Note(s) : NONE

Parameters		: ch -	character to print

Return value	: NONE
*********************************************************************************************/
void HC06_PutChar(uint16_t ch)
{
	/* Put character on the serial line */
	USART_SendData(USART, ch);
	/* Loop until transmit data register is empty */
	 while( !(USART->SR & 0x00000040) );
}

/*********************************************************************************************
Function name   : HC06_PutStr
Author 			: Grant Phillips
Date Modified   : 05/08/2013
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Writes a string to the HC-06 Bluetooth module.

Special Note(s) : NONE

Parameters		: str - string (char array) to print

Return value	: NONE
*********************************************************************************************/
void HC06_PutStr(char *str)
{
	int i;
	for(i = 0; str[i] != '\0'; i++)
		HC06_PutChar(str[i]);
}

/*********************************************************************************************
Function name   : HC06_ClearRxBuffer
Author 			: Grant Phillips
Date Modified   : 05/08/2013
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Clears the software Rx buffer for the HC-06.

Special Note(s) : NONE

Parameters		: ch -	character to print

Return value	: NONE
*********************************************************************************************/
void HC06_ClearRxBuffer(void)
{
	memset(HC06_rx_buffer, 0, HC06_RX_BUFFER_LENGTH); //clear HC06_rx_buffer
	HC06_rx_counter = 0; //reset the Rx buffer counter
	new_HC06_msg = 0; //reset new message flag
}

/*********************************************************************************************
Function name   : HC06_Test
Author 			: Grant Phillips
Date Modified   : 06/08/2013
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Tests if there is communications with the HC-06.

Special Note(s) : NONE

Parameters		: NONE

Return value	: 0		-		Success
				  1		-		Timeout error; not enough characters received for "OK" message
				  2		-		enough characters received, but incorrect message
*********************************************************************************************/
uint8_t HC06_Test(void)
{
	uint32_t timeout = HC06_TIMEOUT_MAX;
	
	HC06_ClearRxBuffer(); //clear rx buffer
	HC06_PutStr("AT"); //AT command for TEST COMMUNICATIONS
	while(HC06_rx_counter < 2) //wait for "OK" - i.e. waiting for 2 characters
	{
		timeout--;
		HC06_Delay(1000); //wait +/- 100us just to give interrupt time to service incoming message
		if (timeout == 0) 
			return 0x01; //if the timeout delay is exeeded, exit with error code
	}
	if(strcmp(HC06_rx_buffer, "OK") == 0)
		return 0x00; //success
	else
		return 0x02; //unknown return AT msg from HC06
}

/*********************************************************************************************
Function name   : HC06_SetBaud
Author 			: Grant Phillips
Date Modified   : 06/08/2013
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Set the default Baud rate for the HC-06.

Special Note(s) : NONE

Parameters		: speed - 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 or 230400

Return value	: 0		-		Success
				  1		-		Incorrect speed selected/typed
				  2		-		Timeout error; not enough characters received for "OKxxxx" message
				  3		-		enough characters received, but incorrect message
*********************************************************************************************/
uint8_t HC06_SetBaud(uint32_t speed)
{
	uint32_t timeout = HC06_TIMEOUT_MAX;
	char buf[20];
	
	HC06_ClearRxBuffer(); //clear rx buffer
	//AT command for SET BAUD speed
	if(speed == 1200)
	{
		strcpy(buf, "OK1200");
		HC06_PutStr("AT+BAUD1");													
	}
	else if(speed == 2400)
	{
		strcpy(buf, "OK2400");
		HC06_PutStr("AT+BAUD2");													
	}
	else if(speed == 4800)
	{
		strcpy(buf, "OK4800");
		HC06_PutStr("AT+BAUD3");													
	}
	else if(speed == 9600)
	{
		strcpy(buf, "OK9600");
		HC06_PutStr("AT+BAUD4");													
	}
	else if(speed == 19200)
	{
		strcpy(buf, "OK19200");
		HC06_PutStr("AT+BAUD5");													
	}
	else if(speed == 38400)
	{
		strcpy(buf, "OK38400");
		HC06_PutStr("AT+BAUD6");													
	}
	else if(speed == 57600)
	{
		strcpy(buf, "OK57600");
		HC06_PutStr("AT+BAUD7");													
	}
	else if(speed == 115200)
	{
		strcpy(buf, "OK115200");
		HC06_PutStr("AT+BAUD8");													
	}
	else if(speed == 230400)
	{
		strcpy(buf, "OK230400");
		HC06_PutStr("AT+BAUD9");													
	}
	else
	{
		return 0x01; //error - incorrect speed
	}

	while(HC06_rx_counter < strlen(buf)) //wait for "OK" message
	{
		timeout--;
		HC06_Delay(1000); //wait +/- 100us just to give interrupt time to service incoming message
		if (timeout == 0) 
			return 0x02; //if the timeout delay is exeeded, exit with error code
	}
	if(strcmp(HC06_rx_buffer, buf) == 0)
		return 0x00; //success
	else
		return 0x03; //unknown return AT msg from HC06
}

/*********************************************************************************************
Function name   : HC06_SetName
Author 					: Grant Phillips
Date Modified   : 06/08/2013
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Set the default Bluetooth name for the HC-06.

Special Note(s) : NONE

Parameters		: name - string that represents the new name (up to 20 characters)

Return value	: 0		-		Success
				  1		-		error - more than 13 characters used for name
				  2		-		Timeout error; not enough characters received for "OKsetname" message
				  3		-		enough characters received, but incorrect message
*********************************************************************************************/
uint8_t HC06_SetName(char *name)
{
	uint32_t timeout = HC06_TIMEOUT_MAX;
	char cmd[20];
	
	HC06_ClearRxBuffer(); //clear rx buffer
	
	if(strlen(name) > 13) //error - name more than 20 characters
		return 0x01;
	
	sprintf(cmd, "AT+NAME%s", name);
	HC06_PutStr(cmd); //AT command for SET NAME
	
	while(HC06_rx_counter < 9) //wait for "OKsetname" message, i.e. 9 chars
	{
		timeout--;
		HC06_Delay(1000); //wait +/- 100us just to give interrupt time to service incoming message
		if (timeout == 0) 
			return 0x02; //if the timeout delay is exeeded, exit with error code
	}
	if(strcmp(HC06_rx_buffer, "OKsetname") == 0)
		return 0x00; //success
	else
		return 0x03; //unknown return AT msg from HC06
}

/*********************************************************************************************
Function name   : HC06_SetPin
Author 			: Grant Phillips
Date Modified   : 06/08/2013
Compiler        : Keil ARM-MDK (uVision V4.70.0.0)

Description		: Set the default Bluetooth name for the HC-06.

Special Note(s) : NONE

Parameters: pin - string that represents the new pin number (must be 4 characters); must
			be represented by "0" - "9" characters, e.g. "1234"

Return value	: 0		-		Success
			  	  1		-		pin less than or more than 4 characters or/and not valid
								characters ("0" - "9")
			  	  2		-		Timeout error; not enough characters received for "OKsetPIN" message
			  	  3		-		enough characters received, but incorrect message
*********************************************************************************************/
uint8_t HC06_SetPin(char *pin)
{
	uint32_t timeout = HC06_TIMEOUT_MAX;
	char buf[20];
	
	HC06_ClearRxBuffer(); //clear rx buffer
	
	if((strlen(pin) < 4) || (strlen(pin) > 4))
		return 0x01; //error - too few or many characetrs in pin
			
	sprintf(buf, "AT+PIN%s", pin);
	HC06_PutStr(buf); //AT command for SET PIN
	
	while(HC06_rx_counter < 8) //wait for "OKsetpin" message, i.e. 8 chars
	{
		timeout--;
		HC06_Delay(1000); //wait +/- 100us just to give interrupt time to service incoming message
		if (timeout == 0) 
			return 0x02; //if the timeout delay is exeeded, exit with error code
	}
	if(strcmp(HC06_rx_buffer, "OKsetPIN") == 0)
		return 0x00; //success
	else
		return 0x03; //unknown return AT msg from HC06
}
