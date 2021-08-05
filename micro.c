/***************************************************************************//**
 * @file
 * @brief EFM micro specific full HAL functions
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include PLATFORM_HEADER
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_core.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_prs.h"
#include "em_cryotimer.h"
#include "sleep-efm32.h"
#include "sl_sleeptimer.h"
#include "em_chip.h"
#include "gpiointerrupt.h"
#include "sl_mpu.h"

#include "stack/include/ember.h"
#include "include/error.h"
#include "hal/hal.h"
#include "serial/serial.h"
#include "hal/micro/cortexm3/diagnostic.h"
#include "hal/micro/cortexm3/memmap.h"
#include "hal/micro/cortexm3/flash.h"
#include "cstartup-common.h"
#include "coexistence/protocol/ieee802154/coexistence-802154.h"

#if defined (_EFR_DEVICE)
#include "tempdrv.h"
#include "sleep-efm32.h"
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
#include "sl_hfxo_manager.h"
#endif

#ifdef BSP_STK
#include "bsp.h"
#endif

#ifdef RTOS
  #include "rtos/rtos.h"
#endif

#ifdef HAL_FEM_ENABLE
  #include "util/plugin/plugin-common/fem-control/fem-control.h"
#endif

extern void halStackRadioHoldOffPowerDown(void); // fwd ref
extern void halStackRadioHoldOffPowerUp(void);   // fwd ref

// Declares the PA curves only if we're in RAIL
#if     (PHY_RAIL || PHY_DUALRAIL)
#include "rail.h"
#include "../plugin/pa-conversions/pa_conversions_efr32.h"

#if !PHY_RAIL_MP && !PHY_RAILGB_MP && !defined(EMBER_STACK_CONNECT)
// Stubs to deadstrip RAIL code. This is not safe in mulitprotocol, since the
// other PHYs could be using old PHYs or frame-type PHYs. These stubs will be
// generated by the calculator in those cases.
uint32_t RAILCb_CalcSymbolRate(RAIL_Handle_t railHandle)
{
  (void) railHandle;
  return 0U;
}

uint32_t RAILCb_CalcBitRate(RAIL_Handle_t railHandle)
{
  (void) railHandle;
  return 0U;
}

void RAILCb_ConfigFrameTypeLength(RAIL_Handle_t railHandle,
                                  const RAIL_FrameType_t *frameType)
{
  (void) railHandle;
  (void) frameType;
}
#endif //!PHY_RAIL_MP && !PHY_RAILGB_MP && !defined(EMBER_STACK_CONNECT)
#else//!(PHY_RAIL || PHY_DUALRAIL)
#include "rail_types.h"
#endif//(PHY_RAIL || PHY_DUALRAIL)

#if defined (_EFR_DEVICE)

// Provide HAL pointers to board-header-defined PA configuration(s)
// for use by App, RAIL, or PHY library.
#ifdef  HAL_PA_ENABLE
static const RAIL_TxPowerConfig_t paInit2p4 =
{
#if defined (_SILICON_LABS_32B_SERIES_1)
#if HAL_PA_2P4_LOWPOWER
  .mode = RAIL_TX_POWER_MODE_2P4_LP,   /* Power Amplifier mode */
#else
  .mode = RAIL_TX_POWER_MODE_2P4_HP,   /* Power Amplifier mode */
#endif
#else
#ifdef HAL_PA_SELECTION  /* Power Amplifier mode */
#if (HAL_PA_SELECTION == HAL_PA_SELECTION_2P4_HP) && defined(RAIL_TX_POWER_MODE_2P4_HP)
  .mode = RAIL_TX_POWER_MODE_2P4_HP,
#elif (HAL_PA_SELECTION == HAL_PA_SELECTION_2P4_MP) && defined(RAIL_TX_POWER_MODE_2P4_MP)
  .mode = RAIL_TX_POWER_MODE_2P4_MP,           
#elif (HAL_PA_SELECTION == HAL_PA_SELECTION_2P4_LP) && defined(RAIL_TX_POWER_MODE_2P4_LP)
  .mode = RAIL_TX_POWER_MODE_2P4_LP,  
#else
  .mode = RAIL_TX_POWER_MODE_2P4_HIGHEST, 
#endif
#else
  .mode = RAIL_TX_POWER_MODE_2P4_HIGHEST,
#endif
#endif
  .voltage = BSP_PA_VOLTAGE,         /* Power Amplifier vPA Voltage mode */
  .rampTime = HAL_PA_RAMP,           /* Desired ramp time in us */
};
const RAIL_TxPowerConfig_t* halInternalPa2p4GHzInit = &paInit2p4;

#if defined (_SILICON_LABS_32B_SERIES_1)
static const RAIL_TxPowerConfig_t paInitSub =
{
  .mode = RAIL_TX_POWER_MODE_SUBGIG, /* Power Amplifier mode */
  .voltage = BSP_PA_VOLTAGE,         /* Power Amplifier vPA Voltage mode */
  .rampTime = HAL_PA_RAMP,           /* Desired ramp time in us */
};

const RAIL_TxPowerConfig_t* halInternalPaSubGHzInit = &paInitSub;
#else
const RAIL_TxPowerConfig_t* halInternalPaSubGHzInit = NULL;
#endif

#else//!HAL_PA_ENABLE
const RAIL_TxPowerConfig_t* halInternalPa2p4GHzInit = NULL;
const RAIL_TxPowerConfig_t* halInternalPaSubGHzInit = NULL;
#endif//HAL_PA_ENABLE
#endif// _EFR_DEVICE

#if HAL_EZRADIOPRO_ENABLE
#if BSP_EZRADIOPRO_USART == HAL_SPI_PORT_USART0
#define PRO2_USART USART0
#elif BSP_EZRADIOPRO_USART == HAL_SPI_PORT_USART1
#define PRO2_USART USART1
#elif BSP_EZRADIOPRO_USART == HAL_SPI_PORT_USART2
#define PRO2_USART USART2
#elif BSP_EZRADIOPRO_USART == HAL_SPI_PORT_USART3
#define PRO2_USART USART3
#else
#error "Invalid EZRADIOPRO USART"
#endif
const uint8_t pro2SpiClockMHz = HAL_EZRADIOPRO_FREQ / 1000000;
#ifdef  _EFR_DEVICE
#include "spidrv.h"
const SPIDRV_Init_t pro2SpiConfig = {
  .port             = PRO2_USART,
  .portLocationTx   = BSP_EZRADIOPRO_MOSI_LOC,
  .portLocationRx   = BSP_EZRADIOPRO_MISO_LOC,
  .portLocationClk  = BSP_EZRADIOPRO_CLK_LOC,
  .portLocationCs   = 0, //not used by application
  .bitRate          = HAL_EZRADIOPRO_FREQ,
/**** Below fields should NOT be modified by customers ****/
  .frameLength      = 8,
  .dummyTxValue     = 0xFF,
  .type             = spidrvMaster,
  .bitOrder         = spidrvBitOrderMsbFirst,
  .clockMode        = spidrvClockMode0,
  .csControl        = spidrvCsControlApplication,
  .slaveStartMode   = spidrvSlaveStartImmediate,
};
#endif//_EFR_DEVICE
#endif

// halInit is called on first initial boot, not on wakeup from sleep.
void halInit(void)
{
  //When the Cortex-M3 exits reset, interrupts are enable.  Explicitly
  //disable them for the rest of Init.
  __disable_irq();

  // Configure BASEPRI to be at the interrupts disabled level so that when we
  // turn interrupts back on nothing fires immediately.
  INTERRUPTS_OFF();

  // Bootloader might be at the base of flash, or even in the NULL_BTL case,
  // the BAT/AAT will be at the beginning of the image.
  // Setting the vectorTable is required.
  SCB->VTOR =  (uint32_t)halAppAddressTable.baseTable.vectorTable;

  // Always Configure Interrupt Priorities.  This is necessary for key behavior
  // such as fault Handlers to be serviced at the correct priority levels.
  #undef FIXED_EXCEPTION
  #define FIXED_EXCEPTION(vectorNumber, functionName, deviceIrqn, deviceIrqHandler)
  #define EXCEPTION(vectorNumber, functionName, deviceIrqn, deviceIrqHandler, priorityLevel, subpriority) \
  NVIC_SetPriority(deviceIrqn, NVIC_EncodePriority(PRIGROUP_POSITION, priorityLevel, subpriority));
    #include NVIC_CONFIG
  #undef EXCEPTION

  //Now that all the individual priority bits are set, we have to set the
  //distinction between preemptive priority and non-preemptive subpriority
  //This sets the priority grouping binary position.
  //PRIGROUP_POSITION is defined inside of nvic-config.h.
  NVIC_SetPriorityGrouping(PRIGROUP_POSITION);

  // Always Configure System Handlers Control and Configuration
#if defined(SCB_CCR_STKALIGN_Msk)
  SCB->CCR |= SCB_CCR_STKALIGN_Msk;
#endif

#if defined(SCB_CCR_DIV_0_TRP_Msk)
  SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
#endif

#if defined(SCB_SHCSR_USGFAULTENA_Msk)
  SCB->SHCSR = (SCB_SHCSR_USGFAULTENA_Msk
                | SCB_SHCSR_BUSFAULTENA_Msk
                | SCB_SHCSR_MEMFAULTENA_Msk);
#endif

  // On EFR32 we use the Simple MPU component to disable execution from general
  // RAM. An exception is made for the area to which RAM functions (if any) are
  // copied. See Simple MPU documentation for further details.
  // This differs from EM3xx, where the MPU config disallows running from all
  // RAM. The expectation is that the application will disable the MPU around
  // calls to RAM functions. On EM3xx, the MPU is also set up with regions for
  // flash, peripherals, and the guard area between the stack and heap. See
  // the mpu-config.h header for your specific EM3xx variant for more info.
  sl_mpu_disable_execute_from_ram();

  // Determine and record the reason for the reset.  Because this code uses
  // static variables in RAM, it must be performed after RAM segements are
  // initialized, but the RESETINFO segment is left uninitialized.
  halInternalClassifyReset();

  //Fill the unused portion of the memory reserved for the stack.
  //memset() is not being used to do this in case it uses the stack
  //to store the return address.
  volatile uint32_t *dataDestination;
  //This code assumes that the __get_MSP() return value and
  //_CSTACK_SEGMENT_BEGIN are both 32-bit aligned values.
  dataDestination = (uint32_t*) (__get_MSP() - 4U);
  //Start at current stack ptr fill up until CSTACK_SEGMENT_BEGIN
  while (dataDestination >= _CSTACK_SEGMENT_BEGIN) {
    //Fill with magic value interpreted by C-SPY's Stack View
    *dataDestination-- = STACK_FILL_VALUE;
  }

  // Zero out the EMHEAP segment.
  {
    // IAR warns about "integer conversion resulted in truncation" if
    // _EMHEAP_SEGMENT_SIZE is used directly in MEMSET().  This segment
    // should always be smaller than a 16bit size.
    MEMSET(_EMHEAP_SEGMENT_BEGIN, 0, (_EMHEAP_SEGMENT_SIZE & 0xFFFFu));
  }

  __enable_irq();

  /* Configure board. Select either EBI or SPI mode. */
  CHIP_Init();

#if defined(_SILICON_LABS_32B_SERIES_2)
  // Initialize the HFXO manager on Series 2 devices before HFXO initialization
  sl_hfxo_manager_init_hardware();
#endif

#if defined (_EFR_DEVICE)
  EMU_UnlatchPinRetention();
#endif
  halConfigInit();
#if defined (_EFR_DEVICE)
  #ifndef HAL_CONFIG
  halInternalInitBoard();
  #endif
  TEMPDRV_Init();
  EMU_EM4Init_TypeDef em4Init = EMU_EM4INIT_DEFAULT;
  em4Init.em4State = emuEM4Hibernate;
  EMU_EM4Init(&em4Init);
  halInternalEm4Wakeup();
#endif

#ifndef FPGA
  halInternalStartSystemTimer();
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
  // Initialize the HFXO manager after halConfigInit() is done
  sl_hfxo_manager_init();
#endif

  halEnergyModeNotificationInit();

#if (PHY_RAIL || PHY_DUALRAIL)
  (void)RAIL_InitPowerManager();
#if (BSP_PA_VOLTAGE > 1800) || defined (_SILICON_LABS_32B_SERIES_2)
  RAIL_InitTxPowerCurvesAlt(&RAIL_TxPowerCurvesVbat);
#else
  RAIL_InitTxPowerCurvesAlt(&RAIL_TxPowerCurvesDcdc);
#endif
#endif//(PHY_RAIL || PHY_DUALRAIL)
}

void halReboot(void)
{
  halInternalSysReset(RESET_SOFTWARE_REBOOT);
}

void halPowerDown(void)
{
  #if HAL_EZRADIOPRO_SHUTDOWN_SLEEP
  extern void emRadioPowerDown(void);
  emRadioPowerDown();
  #endif
  #if HAL_FEM_ENABLE
  shutdownFem();
  #endif
  #ifndef HAL_CONFIG
  halInternalPowerDownBoard();
  #endif
  halConfigPowerDownGpio();
}

// halPowerUp is called from sleep state, not from first initial boot.
void halPowerUp(void)
{
  halConfigPowerUpGpio();
  #ifndef HAL_CONFIG
  halInternalPowerUpBoard();
  #endif
  #if HAL_FEM_ENABLE
  wakeupFem();
  #endif
  #if HAL_EZRADIOPRO_SHUTDOWN_SLEEP
  extern void emRadioPowerUp(void);
  emRadioPowerUp();
  #endif
}

#ifndef EMBER_APPLICATION_HAS_CUSTOM_SLEEP_CALLBACK
WEAK(void halSleepCallback(boolean enter, SleepModes sleepMode))
{
}

#endif // EMBER_APPLICATION_HAS_CUSTOM_SLEEP_CALLBACK

void halStackRadioPowerDownBoard(void)
{
  // For EFM/EFR32 PHYs the PHY takes care of PTA on radio power-down
  halStackRadioHoldOffPowerDown();
}

void halStackRadio2PowerDownBoard(void)
{
  // Neither PTA nor RHO are supported on Radio2
}

void halStackRadioPowerUpBoard(void)
{
  (void) halPtaStackEvent(PTA_STACK_EVENT_RX_LISTEN, 0U);
  halStackRadioHoldOffPowerUp();
}

void halStackRadio2PowerUpBoard(void)
{
  // Neither PTA nor RHO are supported on Radio2
}

void halStackRadioPowerMainControl(bool powerUp)
{
  if (powerUp) {
    halRadioPowerUpHandler();
  } else {
    halRadioPowerDownHandler();
  }
}

void halStackProcessBootCount(void)
{
  //Note: Because this always counts up at every boot (called from emberInit),
  //and non-volatile storage has a finite number of write cycles, this will
  //eventually stop working.  Disable this token call if non-volatile write
  //cycles need to be used sparingly.
#if defined(CREATOR_STACK_BOOT_COUNTER)
  halCommonIncrementCounterToken(TOKEN_STACK_BOOT_COUNTER);
#endif
}

PGM_P halGetResetString(void)
{
  // Table used to convert from reset types to reset strings.
  #define RESET_BASE_DEF(basename, value, string)  string,
  #define RESET_EXT_DEF(basename, extname, extvalue, string)     /*nothing*/
  static PGM char resetStringTable[][4] = {
    #include "reset-def.h"
  };
  #undef RESET_BASE_DEF
  #undef RESET_EXT_DEF
  uint8_t resetInfo = halGetResetInfo();
  if (resetInfo >= (sizeof(resetStringTable) / sizeof(resetStringTable[0]))) {
    return resetStringTable[0x00];   // return unknown
  } else {
    return resetStringTable[resetInfo];
  }
}

// Note that this API should be used in conjunction with halGetResetString
//  to get the full information, as this API does not provide a string for
//  the base reset type
PGM_P halGetExtendedResetString(void)
{
  // Create a table of reset strings for each extended reset type
  typedef PGM char ResetStringTableType[][4];
  #define RESET_BASE_DEF(basename, value, string) \
  }; static ResetStringTableType basename##ResetStringTable = {
  #define RESET_EXT_DEF(basename, extname, extvalue, string)  string,
  {
    #include "reset-def.h"
  };
  #undef RESET_BASE_DEF
  #undef RESET_EXT_DEF

  // Create a table of pointers to each of the above tables
  #define RESET_BASE_DEF(basename, value, string)  (ResetStringTableType *)basename##ResetStringTable,
  #define RESET_EXT_DEF(basename, extname, extvalue, string)     /*nothing*/
  static ResetStringTableType * PGM extendedResetStringTablePtrs[] = {
    #include "reset-def.h"
  };
  #undef RESET_BASE_DEF
  #undef RESET_EXT_DEF

  uint16_t extResetInfo = halGetExtendedResetInfo();
  // access the particular table of extended strings we are interested in
  ResetStringTableType *extendedResetStringTable =
    extendedResetStringTablePtrs[RESET_BASE_TYPE(extResetInfo)];

  // return the string from within the proper table
  return (*extendedResetStringTable)[((extResetInfo) & 0xFF)];
}

// Translate EM3xx reset codes to the codes previously used by the EM2xx.
// If there is no corresponding code, return the EM3xx base code with bit 7 set.
uint8_t halGetEm2xxResetInfo(void)
{
  uint8_t reset = halGetResetInfo();

  // Any reset with an extended value field of zero is considered an unknown
  // reset, except for FIB resets.
  if ((RESET_EXTENDED_FIELD(halGetExtendedResetInfo()) == 0)
      && (reset != RESET_FIB)) {
    return EM2XX_RESET_UNKNOWN;
  }

  switch (reset) {
    case RESET_UNKNOWN:
      return EM2XX_RESET_UNKNOWN;
    case RESET_BOOTLOADER:
      return EM2XX_RESET_BOOTLOADER;
    case RESET_EXTERNAL:    // map pin resets to poweron for EM2xx compatibility
//    return EM2XX_RESET_EXTERNAL;
    case RESET_POWERON:
      return EM2XX_RESET_POWERON;
    case RESET_WATCHDOG:
      return EM2XX_RESET_WATCHDOG;
    case RESET_SOFTWARE:
      return EM2XX_RESET_SOFTWARE;
    case RESET_CRASH:
      return EM2XX_RESET_ASSERT;
    default:
      return (reset | 0x80);    // set B7 for all other reset codes
  }
}

#include "em_burtc.h"
#include "app/framework/include/af.h"

void BURTC_IRQHandler(void)
{
  BURTC_IntClear(BURTC_IF_COMP); // compare match
}

static uint32_t calculateTimerPeriod(uint32_t duration)
{
  uint32_t comp = duration * 32768;
  comp /= 1024;
  return comp;
}

void setEm4WakeupTimer(uint32_t duration)
{
  CMU_ClockSelectSet(cmuClock_EM4GRPACLK, cmuSelect_LFRCO);

  BURTC_Enable(false);

  BURTC_Init_TypeDef burtcInit = BURTC_INIT_DEFAULT;
  burtcInit.compare0Top = true; // reset counter when counter reaches compare value
  burtcInit.em4comp = true;     // BURTC compare interrupt wakes from EM4 (causes reset)
  BURTC_Init(&burtcInit);
  BURTC_CounterReset();
  uint32_t comp = calculateTimerPeriod(duration);
  BURTC_CompareSet(0, comp);
  BURTC_IntClear(BURTC_IF_COMP);     // compare match
  NVIC_ClearPendingIRQ(BURTC_IRQn);
  BURTC_IntEnable(BURTC_IF_COMP);     // compare match
  NVIC_EnableIRQ(BURTC_IRQn);

  BURTC_Enable(true);
}

void halCommonWriteRtccRam(uint8_t index, void* data, uint8_t len)
{
  // for now we always assume it is an integer we write
  uint32_t *ram = (uint32_t *) data;
  BURAM->RET[index].REG = *ram; // increment EM4 wakeup counter
}

void halCommonReadRtccRam(uint8_t index, void* data, uint8_t len)
{
  // for now we always assume it is an integer we read
  uint32_t *ram = (uint32_t *) data;
  *ram = BURAM->RET[index].REG;
}

/*
   1. write the outgoing nwk counter , incoming parent framecounter into rtcc ram
   2. set wakeup timer
 */
void halBeforeEM4(uint32_t duration, RTCCRamData input)
{
  //read the outgoing NWK counter and write it into rtcc ram
  //the first RTCC register is used for outgoing nwk counter
  // and the second one could be used for incoming nwk counter
  halCommonWriteRtccRam(0, &input.outgoingNwkFrameCounter, 4);
  halCommonWriteRtccRam(1, &input.incomingParentNwkFrameCounter, 4);
  halCommonWriteRtccRam(2, &input.outgoingLinkKeyFrameCounter, 4);
  halCommonWriteRtccRam(3, &input.incomingLinkKeyFrameCounter, 4);
  //set the wakeup timer
  setEm4WakeupTimer(duration);

}

/*
   1. read the outgoing nwk counter, incoming parent framecounter from rtcc ram
 */
RTCCRamData halAfterEM4(void)
{
  RTCCRamData output;
  output.outgoingNwkFrameCounter = 0;
  output.incomingParentNwkFrameCounter = 0;
  uint8_t index = 0;
  halCommonReadRtccRam(index, &output.outgoingNwkFrameCounter, 4);
  index++;
  halCommonReadRtccRam(index, &output.incomingParentNwkFrameCounter, 4);
  index++;
  halCommonReadRtccRam(index, &output.outgoingLinkKeyFrameCounter, 4);
  index++;
  return output;
}


#if defined(CRYOTIMER_PRESENT) && (CRYOTIMER_COUNT == 1)

#define WORD_SIZE 32
#if defined(__IAR_SYSTEMS_ICC__)
#define binLog(value) (WORD_SIZE - 1 - __CLZ(value))
#elif defined(__GNUC__)
#define binLog(value) (WORD_SIZE - 1 - __builtin_clz(value))
#else
#define HIGHMASK 0x80000000
static uint32_t binLog(uint32_t value)
{
  uint32_t count = 0;
  while (!(value & HIGHMASK)) {
    value = value << 1;
    count++;
  }
  return WORD_SIZE - 1 - count;
}
#endif

static uint32_t calculateTimerPeriod(uint32_t duration)
{
  // Add 1 if not a power-of-2 or 0 to round up.
  // https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
  if (duration == 0) {
    return 0;
  }
  uint32_t round = ((duration & (duration - 1U)) == 0U) ? 0U : 1U;
  return round + binLog(duration);
}

void setEm4WakeupTimer(uint32_t duration)
{
  CRYOTIMER_Enable(false);
  CRYOTIMER_Init_TypeDef cryotimerInit = CRYOTIMER_INIT_DEFAULT;
  cryotimerInit.em4Wakeup = true;
  cryotimerInit.osc = cryotimerOscULFRCO;
  cryotimerInit.period = (CRYOTIMER_Period_TypeDef) calculateTimerPeriod(duration);

  CMU_ClockEnable(cmuClock_CORELE, true);
  CMU_ClockEnable(cmuClock_CRYOTIMER, true);

  CRYOTIMER_IntClear(CRYOTIMER_IFC_PERIOD);
  CRYOTIMER_Init(&cryotimerInit);
  CRYOTIMER_Enable(true);
}

void halCommonWriteRtccRam(uint8_t index, void* data, uint8_t len)
{
  // for now we always assume it is an integer we write
  uint32_t *ram = (uint32_t *) data;
  RTCC->RET[index].REG = *ram;
}

void halCommonReadRtccRam(uint8_t index, void* data, uint8_t len)
{
  // for now we always assume it is an integer we read
  uint32_t *ram = (uint32_t *) data;
  *ram = RTCC->RET[index].REG;
}

/*
   1. write the outgoing nwk counter , incoming parent framecounter into rtcc ram
   2. set wakeup timer
 */
void halBeforeEM4(uint32_t duration, RTCCRamData input)
{
  //read the outgoing NWK counter and write it into rtcc ram
  //the first RTCC register is used for outgoing nwk counter
  // and the second one could be used for incoming nwk counter
  halCommonWriteRtccRam(0, &input.outgoingNwkFrameCounter, 4);
  halCommonWriteRtccRam(1, &input.incomingParentNwkFrameCounter, 4);
  halCommonWriteRtccRam(2, &input.outgoingLinkKeyFrameCounter, 4);
  halCommonWriteRtccRam(3, &input.incomingLinkKeyFrameCounter, 4);
  //set the wakeup timer
  setEm4WakeupTimer(duration);
}

/*
   1. read the outgoing nwk counter, incoming parent framecounter from rtcc ram
 */
RTCCRamData halAfterEM4(void)
{
  RTCCRamData output;
  output.outgoingNwkFrameCounter = 0;
  output.incomingParentNwkFrameCounter = 0;
  uint8_t index = 0;
  halCommonReadRtccRam(index, &output.outgoingNwkFrameCounter, 4);
  index++;
  halCommonReadRtccRam(index, &output.incomingParentNwkFrameCounter, 4);
  index++;
  halCommonReadRtccRam(index, &output.outgoingLinkKeyFrameCounter, 4);
  index++;
  halCommonReadRtccRam(index, &output.incomingLinkKeyFrameCounter, 4);
  return output;
}
#endif //defined(CRYOTIMER_PRESENT) && (CRYOTIMER_COUNT == 1)
