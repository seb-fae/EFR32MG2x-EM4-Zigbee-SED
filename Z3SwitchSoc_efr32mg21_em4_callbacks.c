
bool emberAfPluginIdleSleepOkToSleepCallback(uint32_t durationMs)
{
  // Disable Watchdog to reach 5 uA of EM2 current. Otherwise EM2 current is 7.5 uA 
  WDOGn_Enable(WDOG0, false);
  return true;
}
bool emberAfPluginEm4EnterCallback(uint32_t* durationMs)
{
  // Do not go to EM4 mode if duration is not sufficient */
  if (*durationMs > 2000)
   return true;
  return false;
}

