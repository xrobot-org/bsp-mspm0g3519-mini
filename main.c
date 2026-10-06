// SDK empty example plus app_main(). SYSCFG_DL_init() sets up clocks, pins and
// peripherals from the .syscfg; app_main() (User/app_main.cpp) creates the LibXR objects,
// which own their interrupts and interrupt priorities.
#include "app_main.h"
#include "ti_msp_dl_config.h"

int main(void)
{
  SYSCFG_DL_init();

  app_main();

  return 0;
}
