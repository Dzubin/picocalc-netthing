/*
 * platform_pico.c - platform.h on the Raspberry Pi Pico SDK (PicoCalc
 * firmware, Pico 2 W).
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/rand.h"
#include "hardware/watchdog.h"

#include "platform.h"
#include "platform_pico_const.h"

void plat_init(void)
{
    stdio_init_all();
}

uint32_t plat_now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void plat_sleep_ms(uint32_t ms)
{
    sleep_ms(ms);
}

uint32_t plat_random32(void)
{
    return get_rand_32();
}

_Noreturn void plat_bootsel(void)
{
    reset_usb_boot(0, 0);
    for (;;)
        tight_loop_contents();
}

_Noreturn void plat_exit_to_loader(void)
{
    watchdog_hw->scratch[LOADER_SCRATCH_MODE] = LOADER_BOOT_MODE_SD;
    watchdog_hw->scratch[LOADER_SCRATCH_ARGUMENT] = 0;
    watchdog_hw->scratch[LOADER_SCRATCH_MAGIC] = LOADER_COMMAND_MAGIC;
    watchdog_reboot(0, 0, LOADER_REBOOT_DELAY_MS);
    for (;;)
        tight_loop_contents();      /* the reboot comes in a few ms */
}
