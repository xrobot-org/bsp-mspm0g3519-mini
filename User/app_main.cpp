// LibXR objects and hardware registration of the MSPM0 MINI-D.B.48 BSP.
// Clocks, pins, DMA channels and peripherals are configured by SysConfig
// (mspm0g3519_minidb48.syscfg, run in main.c by SYSCFG_DL_init()). Put application code
// between the "User Code Begin" and "User Code End" markers; the layout follows
// bsp_stm32f103.
#include "app_main.h"

#include <atomic>
#include <cstdint>

#include "libxr.hpp"
#include "mspm0_gpio.hpp"
#include "mspm0_i2c.hpp"
#include "mspm0_pwm.hpp"
#include "mspm0_spi.hpp"
#include "mspm0_timebase.hpp"
#include "mspm0_uart.hpp"
#include "ti_msp_dl_config.h"
#include "xrobot_main.hpp"

using namespace LibXR;

/* User Code Begin 1 */
// KEY interrupt counters, incremented in the GPIO interrupt callbacks.
static std::atomic<uint32_t> key1_irq_count{0};
static std::atomic<uint32_t> key2_irq_count{0};

static void OnKeyInterrupt(bool in_isr, std::atomic<uint32_t>* count)
{
  (void)in_isr;
  count->fetch_add(1, std::memory_order_relaxed);
}
/* User Code End 1 */

// The I2C and SPI modules run from BUSCLK (ULPCLK); SysConfig does not export its
// frequency, so take it from UART_0, which uses BUSCLK as well. (MSPM0_I2C_INIT and
// MSPM0_SPI_INIT would pass CPUCLK_FREQ, which is twice as high at MCLK = 80 MHz.)
static constexpr uint32_t BUSCLK_FREQ = UART_0_INST_FREQUENCY;

// BSP declarations for MSPM0_UART_MAIN_INIT: the UART that owns each DMA channel, and
// that the channel serves the transmitter. The macro checks them against the SysConfig
// names.
#define DMA_CH_UART0_TX_LIBXR_UART_IRQN UART_0_INST_INT_IRQN
#define DMA_CH_UART0_TX_LIBXR_UART_TX 1
#define DMA_CH_UART1_TX_LIBXR_UART_IRQN UART_1_INST_INT_IRQN
#define DMA_CH_UART1_TX_LIBXR_UART_TX 1
#define DMA_CH_UART7_TX_LIBXR_UART_IRQN UART_7_INST_INT_IRQN
#define DMA_CH_UART7_TX_LIBXR_UART_TX 1

// DMA buffers. A UART gets 2 x N bytes for its two transmit halves; the receive side
// (byte interrupts) has no DMA buffer. SPI DMA buffers are split in two halves as well.
// The I2C driver uses polling, its buffer is the staging area for DMA transfers.
alignas(size_t) static uint8_t uart0_tx_buf[2 * 128];
alignas(size_t) static uint8_t uart1_tx_buf[2 * 64];
alignas(size_t) static uint8_t uart7_tx_buf[2 * 64];
alignas(4) static uint8_t spi1_rx_buf[64];
alignas(4) static uint8_t spi1_tx_buf[64];
alignas(4) static uint8_t i2c0_buf[32];
alignas(4) static uint8_t i2c1_buf[32];

extern "C" void app_main(void)
{
  /* User Code Begin 2 */
  /* User Code End 2 */

  // Timebase and platform
  static MSPM0Timebase timebase;
  PlatformInit();

  // GPIO: SysConfig configured LED1 (PB8) and LED2 (PA16) as outputs, high (off, active
  // low), and KEY1 (PB24) and KEY2 (PB20) as inputs.
  static MSPM0GPIO LED1(GPIO_LEDS_PIN_LED1_PORT, GPIO_LEDS_PIN_LED1_PIN,
                        GPIO_LEDS_PIN_LED1_IOMUX);
  static MSPM0GPIO LED2(GPIO_LEDS_PIN_LED2_PORT, GPIO_LEDS_PIN_LED2_PIN,
                        GPIO_LEDS_PIN_LED2_IOMUX);
  static MSPM0GPIO KEY1(GPIO_KEYS_PORT, GPIO_KEYS_PIN_KEY1_PIN, GPIO_KEYS_PIN_KEY1_IOMUX);
  static MSPM0GPIO KEY2(GPIO_KEYS_PORT, GPIO_KEYS_PIN_KEY2_PIN, GPIO_KEYS_PIN_KEY2_IOMUX);
  // The keys pull the pin low against an external 15k pull-up: falling edge, no internal
  // pull.
  KEY1.SetConfig({GPIO::Direction::FALL_INTERRUPT, GPIO::Pull::NONE});
  KEY1.RegisterCallback(GPIO::Callback::Create(OnKeyInterrupt, &key1_irq_count));
  KEY1.EnableInterrupt();
  KEY2.SetConfig({GPIO::Direction::FALL_INTERRUPT, GPIO::Pull::NONE});
  KEY2.RegisterCallback(GPIO::Callback::Create(OnKeyInterrupt, &key2_irq_count));
  KEY2.EnableInterrupt();

  // UART: UART0 (CH340 through the jumpers), UART1 and UART7 (FPC F10/F9); TX with DMA
  static MSPM0UART uart0(MSPM0_UART_MAIN_INIT(UART_0, DMA_CH_UART0_TX, uart0_tx_buf,
                                              sizeof(uart0_tx_buf), 5, 128));
  static MSPM0UART uart1(MSPM0_UART_MAIN_INIT(UART_1, DMA_CH_UART1_TX, uart1_tx_buf,
                                              sizeof(uart1_tx_buf), 5, 64));
  static MSPM0UART uart7(MSPM0_UART_MAIN_INIT(UART_7, DMA_CH_UART7_TX, uart7_tx_buf,
                                              sizeof(uart7_tx_buf), 5, 64));

  // I2C: polling
  static MSPM0I2C i2c0({I2C_0_INST, I2C_0_INST_INT_IRQN, BUSCLK_FREQ, I2C_0_BUS_SPEED_HZ,
                        MSPM0I2C::ResolveIndex(I2C_0_INST_INT_IRQN)},
                       {i2c0_buf, sizeof(i2c0_buf)}, 8, {I2C_0_BUS_SPEED_HZ});
  static MSPM0I2C i2c1({I2C_1_INST, I2C_1_INST_INT_IRQN, BUSCLK_FREQ, I2C_1_BUS_SPEED_HZ,
                        MSPM0I2C::ResolveIndex(I2C_1_INST_INT_IRQN)},
                       {i2c1_buf, sizeof(i2c1_buf)}, 8, {I2C_1_BUS_SPEED_HZ});

  // SPI: DMA for transfers longer than 3 bytes
  static MSPM0SPI spi1({SPI_1_INST, SPI_1_INST_INT_IRQN, BUSCLK_FREQ,
                        MSPM0SPI::ResolveIndex(SPI_1_INST_INT_IRQN),
                        DMA_CH_SPI1_RX_CHAN_ID, DMA_CH_SPI1_TX_CHAN_ID},
                       {spi1_rx_buf, sizeof(spi1_rx_buf)},
                       {spi1_tx_buf, sizeof(spi1_tx_buf)}, 3);

  // PWM: the two channels of TIMA1 share one period; 1 kHz, counter stopped
  static MSPM0PWM pwm_tima1_c0(
      {PWM_TIMA1_INST, GPIO_PWM_TIMA1_C0_IDX, PWM_TIMA1_INST_CLK_FREQ});
  static MSPM0PWM pwm_tima1_c1(
      {PWM_TIMA1_INST, GPIO_PWM_TIMA1_C1_IDX, PWM_TIMA1_INST_CLK_FREQ});
  pwm_tima1_c0.SetConfig({1000});

  // Terminal on uart0
  STDIO::read_ = uart0.read_port_;
  STDIO::write_ = uart0.write_port_;
  static RamFS ramfs("XRobot");
  static Terminal<32, 32, 5, 5> terminal(ramfs);
  static auto terminal_task = Timer::CreateTask(terminal.TaskFun, &terminal, 10);
  Timer::Add(terminal_task);
  Timer::Start(terminal_task);

  // Hardware registration
  XR_REGISTER(LED1, LibXR::GPIO);
  XR_REGISTER(LED2, LibXR::GPIO);
  XR_REGISTER(KEY1, LibXR::GPIO);
  XR_REGISTER(KEY2, LibXR::GPIO);

  XR_REGISTER(pwm_tima1_c0, LibXR::PWM);
  XR_REGISTER(pwm_tima1_c1, LibXR::PWM);

  XR_REGISTER(spi1, LibXR::SPI);

  XR_REGISTER(uart0, LibXR::UART);
  XR_REGISTER(uart1, LibXR::UART);
  XR_REGISTER(uart7, LibXR::UART);

  XR_REGISTER(i2c0, LibXR::I2C);
  XR_REGISTER(i2c1, LibXR::I2C);

  XR_REGISTER(ramfs, LibXR::RamFS);

  XR_REGISTER(terminal, LibXR::Terminal<32, 32, 5, 5>);

  /* User Code Begin 3 */
  /* User Code End 3 */
  XROBOT_MAIN();
}
