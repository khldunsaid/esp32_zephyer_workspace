/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <app/drivers/blink.h>

#include <zephyr/app_version.h>

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);


int main(void)
{
	LOG_INF("Hello World esp32 V: %s\n", APP_VERSION_STRING);
	return 0;
}

