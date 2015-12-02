#include "stm32f4xx_rcc.h"
#include "hc06.h"

int main(void)
{
	/***** IMPORTANT: *****/
	/* Before running SystemInit(); make the necessary changes as stated in the following URL:
	 * http://stackoverflow.com/a/21485189  */
	/**********************/
	SystemInit();
	SystemCoreClockUpdate();
	HC06_Init(115200); /* Set Baud Rate */

	HC06_PutStr("Hello World!");

	while(1);
}
