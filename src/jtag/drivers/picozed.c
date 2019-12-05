/***************************************************************************
 *   Copyright (C) 2006 by Anders Larsen                                   *
 *   al@alarsen.net                                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <jtag/interface.h>
#include "bitbang.h"

#include <sys/mman.h>

/* PICOZED */
#define JTAG_REG mmap_regbase[26]

#define JTAG_TDO  0x80000000
#define JTAG_TDI  0x00000001
#define JTAG_SW   0x00000002
#define JTAG_TRST 0x00000004
#define JTAG_TMS  0x00000008
#define JTAG_TCK  0x00000010

struct device_t {
	 const char *name;
};

static const struct device_t devices[] = {
	{ "psoc" },
	{ "cpld" },
	{ .name = NULL },
};

/* interface variables
 */
static const struct device_t *device = NULL;
static int dev_mem_fd = -1;
static volatile uint32_t * mmap_regbase = NULL;

/* low level command set
 */
static bb_value_t picozed_read(void);
static int picozed_write(int tck, int tms, int tdi);
static int picozed_reset(int trst, int srst);

static int picozed_init(void);
static int picozed_quit(void);

static struct bitbang_interface picozed_bitbang = {
	.read = picozed_read,
	.write = picozed_write,
	.reset = picozed_reset,
	.blink = 0
};

static bb_value_t picozed_read(void)
{
	return (JTAG_REG & JTAG_TDO) ? BB_HIGH : BB_LOW;
}

static int picozed_write(int tck, int tms, int tdi)
{
	uint32_t val = JTAG_REG;
	if (tck)
		val |= JTAG_TCK;
	else
		val &= ~JTAG_TCK;

	if (tms)
		val |= JTAG_TMS;
	else
		val &= ~JTAG_TMS;

	if (tdi)
		val |= JTAG_TDI;
	else
		val &= ~JTAG_TDI;

	JTAG_REG = val;
	return ERROR_OK;
}

/* (1) assert or (0) deassert reset lines */
static int picozed_reset(int trst, int srst)
{
	if (trst == 0)
		JTAG_REG |= JTAG_TRST;
	else if (trst == 1)
		JTAG_REG &= ~JTAG_TRST;

	return ERROR_OK;
}

COMMAND_HANDLER(picozed_handle_device_command)
{
	if (CMD_ARGC == 0)
		return ERROR_COMMAND_SYNTAX_ERROR;


	const struct device_t *cur_device = devices;

	while (cur_device->name) {
		if (strcmp(cur_device->name, CMD_ARGV[0]) == 0) {
			device = cur_device;
			break;
		}
		cur_device++;
	}

	if (!cur_device->name) {
		LOG_ERROR("No matching device found for %s", CMD_ARGV[0]);
		return ERROR_FAIL;
	}
	
	if(dev_mem_fd < 0 || !mmap_regbase)
	{
		LOG_ERROR("JTAG pins have not been initialized");
		return ERROR_FAIL;
	}

	LOG_WARNING("Device is %s", device->name);
	if(device == &devices[0])
	{
		JTAG_REG |= JTAG_SW; // PSOC
	}
	else if(device == &devices[1])
	{
		JTAG_REG &= ~JTAG_SW; // CPLD
	}
	else
	{
		LOG_ERROR("Device not recognized");
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static const struct command_registration picozed_exec_command_handlers[] = {
	{
		.name = "device",
		.handler = &picozed_handle_device_command,
		.mode = COMMAND_EXEC,
		.help = "Set picozed device [default \"psoc\"]",
		.usage = "piczed device <device>",
	},
	COMMAND_REGISTRATION_DONE
};

static const struct command_registration picozed_command_handlers[] = {
	{
		.name = "picozed",
		.mode = COMMAND_ANY,
		.help = "picozed command group",
		.usage = "",
		.chain = picozed_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

struct jtag_interface picozed_interface = {
	.name = "picozed",
	.execute_queue = bitbang_execute_queue,
	.transports = jtag_only,
	.commands = picozed_command_handlers,
	.init = picozed_init,
	.quit = picozed_quit,
};

static int picozed_init(void)
{
	device = &devices[0];
	bitbang_interface = &picozed_bitbang;

	dev_mem_fd = open("/dev/lptdma0", O_RDWR);
	if (dev_mem_fd < 0) {
		perror("open");
		return ERROR_JTAG_INIT_FAILED;
	}

	mmap_regbase = (uint32_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE,
				MAP_SHARED, dev_mem_fd, 0);
	if (mmap_regbase == MAP_FAILED) {
		perror("mmap");
		close(dev_mem_fd);
		return ERROR_JTAG_INIT_FAILED;
	}

	/*
	 * Configure TDO as an input, and TDI, TCK, TMS, TRST, SRST
	 * as outputs.  Drive TDI and TCK low, and TMS/TRST/SRST high.
	 */
	JTAG_REG |= JTAG_SW; // PSOC
	picozed_write(0, 1, 0);
	picozed_reset(1, 1);

	return ERROR_OK;
}

static int picozed_quit(void)
{
	return ERROR_OK;
}
