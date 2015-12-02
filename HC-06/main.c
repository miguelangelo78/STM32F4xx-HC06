#include "stm32f4xx_rcc.h"
#include "hc06.h"

/* NOTE: Somehow, the HC-06 does not respond to STM32F4 when commands are sent.
 * However, it answers the same commands but from an Arduino, which is annoying.
 * We will fix this. */

int main(void)
{
	/***** IMPORTANT: *****/
	/* Before running SystemInit(); make the necessary changes as stated in the following URL:
	 * http://stackoverflow.com/a/21485189 */
	/**********************/
	SystemInit();
	SystemCoreClockUpdate();
	HC06_Init(115200); /* Set Baud Rate */

	HC06_PutStr("Hello World!");

	while(1);
}
