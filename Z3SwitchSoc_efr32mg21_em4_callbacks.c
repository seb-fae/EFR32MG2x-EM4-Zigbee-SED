
bool emberAfPluginIdleSleepOkToSleepCallback(uint32_t durationMs)
{
  WDOGn_Enable(WDOG0, false);
  return true;
}
bool emberAfPluginEm4EnterCallback(uint32_t* durationMs)
{
  if (*durationMs > 2000)
   return true;
  return false;
}

