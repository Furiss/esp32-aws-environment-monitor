#include "wdt.h"
#include <soc/timer_group_struct.h>
#include <soc/timer_group_reg.h>

static void stretch(timg_dev_t* tg)
{
    tg->wdt_wprotect = TIMG_WDT_WKEY_VALUE;
    tg->wdt_config1.clk_prescale = 0xFFFF;
    tg->wdt_config2 = 0xFFFFFFFF;
    tg->wdt_config3 = 0xFFFFFFFF;
    tg->wdt_feed = 1;
    tg->wdt_wprotect = 0;
}

void wdt_silence_for_sleep()
{
    stretch(&TIMERG0);
    stretch(&TIMERG1);
}
