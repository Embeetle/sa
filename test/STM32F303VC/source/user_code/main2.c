#include "main.h"
#include "stm32f3xx_hal.h"
#include "cmsis_os.h"


osThreadId defaultTaskHandle;

static void MX_USB_PCD_Init(void);

int main(void)
{
  MX_USB_PCD_Init();
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);
  extern int abcde;
  
}
