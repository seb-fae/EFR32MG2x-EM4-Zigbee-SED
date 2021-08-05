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

