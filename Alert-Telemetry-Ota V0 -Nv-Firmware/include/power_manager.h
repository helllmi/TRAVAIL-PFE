#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_pm.h>
#include "config.h"


void power_init();

void power_setcpufreq(uint32_t freqMHz);

void power_enterstandby();

void power_enteraction();

uint32_t power_getcpufreq();
#endif