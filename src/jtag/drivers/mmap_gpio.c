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

/* MMAP_GPIO */
#define JTAG_REG_TDO  mmap_regbase[pins.tdo_byte]
#define JTAG_REG_TDI  mmap_regbase[pins.tdi_byte]
#define JTAG_REG_TRST mmap_regbase[pins.trst_byte]
#define JTAG_REG_TMS  mmap_regbase[pins.tms_byte]
#define JTAG_REG_TCK  mmap_regbase[pins.tck_byte]

#define JTAG_TDO  pins.tdo_bit
#define JTAG_TDI  (1 << pins.tdi_bit )
#define JTAG_TRST (1 << pins.trst_bit)
#define JTAG_TMS  (1 << pins.tms_bit )
#define JTAG_TCK  (1 << pins.tck_bit )


struct jtag_pins
{
	int tdo_byte;
	int tdo_bit;
	int tdi_byte;
	int tdi_bit;
	int trst_byte;
	int trst_bit;
	int tms_byte;
	int tms_bit;
	int tck_byte;
	int tck_bit;
};

/* interface variables
 */
static int dev_mem_fd = -1;
static uint8_t * mmap_regbase = NULL;
static int mmap_size = -1;
static struct jtag_pins pins = {0};

/* low level command set
 */
static bb_value_t mmap_gpio_read(void);
static int mmap_gpio_write(int tck, int tms, int tdi);
static int mmap_gpio_reset(int trst, int srst);

static int mmap_gpio_init(void);
static int mmap_gpio_quit(void);

static struct bitbang_interface mmap_gpio_bitbang = {
	.read = mmap_gpio_read,
	.write = mmap_gpio_write,
	.reset = mmap_gpio_reset,
	.blink = 0
};

static bb_value_t mmap_gpio_read(void)
{
	return ((JTAG_REG_TDO >> JTAG_TDO) & 1) ? BB_HIGH : BB_LOW;
}

static int mmap_gpio_write(int tck, int tms, int tdi)
{
	if (tck)
		JTAG_REG_TCK |= JTAG_TCK;
	else
		JTAG_REG_TCK &= ~JTAG_TCK;

	if (tms)
		JTAG_REG_TMS |= JTAG_TMS;
	else
		JTAG_REG_TMS &= ~JTAG_TMS;

	if (tdi)
		JTAG_REG_TDI |= JTAG_TDI;
	else
		JTAG_REG_TDI &= ~JTAG_TDI;

	return ERROR_OK;
}

/* (1) assert or (0) deassert reset lines */
static int mmap_gpio_reset(int trst, int srst)
{
	if (trst == 0)
		JTAG_REG_TRST |= JTAG_TRST;
	else if (trst == 1)
		JTAG_REG_TRST &= ~JTAG_TRST;

	return ERROR_OK;
}

static int mmap_gpio_init(void)
{
	bitbang_interface = &mmap_gpio_bitbang;

	mmap_gpio_write(0, 1, 0);
	mmap_gpio_reset(1, 1);

	return ERROR_OK;
}

static int mmap_gpio_quit(void)
{
	if(mmap_regbase && mmap_regbase != MAP_FAILED)
	{
		munmap(mmap_regbase, mmap_size);
		mmap_regbase = NULL;
	}

	if(dev_mem_fd >= 0)
	{
		close(dev_mem_fd);
		dev_mem_fd = -1;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(mmap_gpio_handle_config_command)
{
	if (CMD_ARGC != 12)
	{
		LOG_ERROR("Invalid number of arguments.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	dev_mem_fd = open(CMD_ARGV[0], O_RDWR);
	if (dev_mem_fd < 0) {
		LOG_ERROR("Cannot open memory mapped file %s.", CMD_ARGV[0]);
		return ERROR_JTAG_INIT_FAILED;
	}

	int sz = atoi(CMD_ARGV[1]);
	if(sz < 0)
		return ERROR_FAIL;
	mmap_size = sz;

	mmap_regbase = (uint8_t *)mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev_mem_fd, 0);
	if (mmap_regbase == MAP_FAILED) {
		LOG_ERROR("Cannot allocate space for memory mapped file.");
		close(dev_mem_fd);
		return ERROR_JTAG_INIT_FAILED;
	}
	
	for(int i = 0; i < 10; i += 2)
	{
		int byte = atoi(CMD_ARGV[i + 2]);
		if(byte < 0 || byte >= mmap_size)
			return ERROR_FAIL;

		int bit = atoi(CMD_ARGV[i + 3]);
		if(bit < 0 || bit >= 8)
			return ERROR_FAIL;

		((int*)&pins)[i] = byte;
		((int*)&pins)[i + 1] = bit;
	}

	LOG_DEBUG("mmap_gpio config:\n"
		"\ttdo_byte %d\n"
		"\ttdo_bit  %d\n"
		"\ttdi_byte %d\n"
		"\ttdi_bit  %d\n"
		"\ttrst_byte %d\n"
		"\ttrst_bit %d\n"
		"\ttms_byte %d\n"
		"\ttms_bit  %d\n"
		"\ttck_byte %d\n"
		"\ttck_bit  %d\n",
		pins.tdo_byte,
		pins.tdo_bit ,
		pins.tdi_byte,
		pins.tdi_bit ,
		pins.trst_byte,
		pins.trst_bit,
		pins.tms_byte,
		pins.tms_bit ,
		pins.tck_byte,
		pins.tck_bit );
	return ERROR_OK;
}

COMMAND_HANDLER(mmap_gpio_handle_set_pin_command)
{
	if (CMD_ARGC != 3)
	{
		LOG_ERROR("Invalid number of arguments.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	if(!mmap_regbase)
	{
		LOG_ERROR("Must call mmap_gpio_config before setting any pins.");
		return ERROR_FAIL;
	}

	int byte = atoi(CMD_ARGV[0]);
	if(byte < 0 || byte >= mmap_size)
		return ERROR_FAIL;

	int bit = atoi(CMD_ARGV[1]);
	if(bit < 0 || bit >= 8)
		return ERROR_FAIL;

	int value = atoi(CMD_ARGV[2]);
	if(value != 0 && value != 1)
		return ERROR_FAIL;

	if (value)
		mmap_regbase[byte] |= 1 << bit;
	else
		mmap_regbase[byte] &= ~(1 << bit);

	LOG_DEBUG("mmap_gpio: Set byte %d, bit %d to %d.", byte, bit, value);
	return ERROR_OK;
}

static const struct command_registration mmap_gpio_command_handlers[] = 
{
	{
		.name = "mmap_gpio_config",
		.handler = &mmap_gpio_handle_config_command,
		.mode = COMMAND_CONFIG,
		.help = "Set memory map and pin configuration.\n"
			"Byte and bit positions must be base 10 integer and represent a byte offset from the beginning of the file and the bit position in that byte.\n"
			"For example, byte 1 and bit 7 is the most siginificant bit of the second byte of the memory mapped file.\n",
		.usage = "<memory mapped file name> <memory mapped file size> <TDO byte> <TDO bit> <TDI byte> <TDI bit> <TRST byte> <TRST bit> <TMS byte> <TMS bit> <TCK byte> <TCK bit>",
	},
	{
		.name = "mmap_gpio_set_pin",
		.handler = &mmap_gpio_handle_set_pin_command,
		.mode = COMMAND_ANY,
		.help = "Set arbitrary pin state.\n"
			"Byte value must be within the configured size.\n"
			"Byte and bit must be base 10 integers, but do not need to be one of the configued jtag pins.\n"
			"Value must be either 0 or 1.",
		.usage = "<byte> <bit> <value>",
	},
	COMMAND_REGISTRATION_DONE
};

struct jtag_interface mmap_gpio_interface = {
	.name = "mmap_gpio",
	.execute_queue = bitbang_execute_queue,
	.transports = jtag_only,
	.commands = mmap_gpio_command_handlers,
	.init = mmap_gpio_init,
	.quit = mmap_gpio_quit,
};
