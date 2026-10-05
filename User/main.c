// Board bring-up of the MSPM0 MINI-D.B.48 BSP.
// SysConfig (mspm0g3519_minidb48.syscfg) sets up the clocks, SysTick (1 ms), pins and
// peripherals in SYSCFG_DL_init(). The LibXR objects for them are created in app_main().
#include "app_main.h"
#include "ti_msp_dl_config.h"

int main(void)
{
  SYSCFG_DL_init();

  // The 1 ms SysTick keeps the LibXR time; it must not wait for any other interrupt.
  // The peripherals LibXR drives with interrupts share the next level, so their handlers
  // never preempt each other (LibXR requires compatible priorities for a UART and its
  // DMA).
  NVIC_SetPriority(SysTick_IRQn, 0U);
  NVIC_SetPriority(UART_0_INST_INT_IRQN, 1U);
  NVIC_SetPriority(UART_1_INST_INT_IRQN, 1U);
  NVIC_SetPriority(UART_7_INST_INT_IRQN, 1U);
  NVIC_SetPriority(SPI_1_INST_INT_IRQN, 1U);
  NVIC_SetPriority(GPIO_KEYS_INT_IRQN, 1U);

  app_main();

  return 0;
}
