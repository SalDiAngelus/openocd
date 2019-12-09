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
#define JTAG_REG_TDO  mmap_regbase[setup_info.pins.name.tdo_byte]
#define JTAG_REG_TDI  mmap_regbase[setup_info.pins.name.tdi_byte]
#define JTAG_REG_TRST mmap_regbase[setup_info.pins.name.trst_byte]
#define JTAG_REG_TMS  mmap_regbase[setup_info.pins.name.tms_byte]
#define JTAG_REG_TCK  mmap_regbase[setup_info.pins.name.tck_byte]

#define JTAG_TDO  setup_info.pins.name.tdo_bit
#define JTAG_TDI  (1 << setup_info.pins.name.tdi_bit )
#define JTAG_TRST (1 << setup_info.pins.name.trst_bit)
#define JTAG_TMS  (1 << setup_info.pins.name.tms_bit )
#define JTAG_TCK  (1 << setup_info.pins.name.tck_bit )


union jtag_pins
{
	int arr[10];
	struct jtag_pin_names
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
	} name;
};

struct jtag_setup
{
	char* file_name;
	int file_size;
	union jtag_pins pins;
};

/* interface variables
 */
static int dev_mem_fd = -1;
static uint8_t * mmap_regbase = NULL;
static struct jtag_setup setup_info = {0};

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
	return ((JTAG_REG_TDO >> JTAG_TDO) & 1) ? BB_HIGH : BB_LOW;
}

static int picozed_write(int tck, int tms, int tdi)
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
static int picozed_reset(int trst, int srst)
{
	if (trst == 0)
		JTAG_REG_TRST |= JTAG_TRST;
	else if (trst == 1)
		JTAG_REG_TRST &= ~JTAG_TRST;

	return ERROR_OK;
}

static int picozed_init(void)
{
	bitbang_interface = &picozed_bitbang;

	picozed_write(0, 1, 0);
	picozed_reset(1, 1);

	LOG_INFO("Picozed init success.");
	return ERROR_OK;
}

static int picozed_quit(void)
{
	if(mmap_regbase && mmap_regbase != MAP_FAILED)
	{
		munmap(mmap_regbase, setup_info.file_size);
		mmap_regbase = NULL;
	}

	if(dev_mem_fd >= 0)
	{
		close(dev_mem_fd);
		dev_mem_fd = -1;
	}

	if(setup_info.file_name)
	{
		free(setup_info.file_name);
		setup_info.file_name = NULL;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(picozed_handle_config_command)
{
	if (CMD_ARGC != 12)
	{
		LOG_ERROR("Invalid number of arguments.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	int fn_len = strlen(CMD_ARGV[0]);
	setup_info.file_name = malloc(fn_len + 1);
	if(!setup_info.file_name)
	{
		LOG_ERROR("1");
		return ERROR_FAIL;
	}
	strcpy(setup_info.file_name, CMD_ARGV[0]);

	int sz = atoi(CMD_ARGV[1]);
	if(sz < 0)
		return ERROR_FAIL;
	setup_info.file_size = sz;


	for(int i = 0; i < 10; i += 2)
	{
		int byte = atoi(CMD_ARGV[i + 2]);
		if(byte < 0 || byte >= setup_info.file_size)
			return ERROR_FAIL;

		int bit = atoi(CMD_ARGV[i + 3]);
		if(bit < 0 || bit >= 8)
			return ERROR_FAIL;

		setup_info.pins.arr[i] = byte;
		setup_info.pins.arr[i + 1] = bit;
	}

	dev_mem_fd = open(setup_info.file_name, O_RDWR);
	if (dev_mem_fd < 0) {
		LOG_ERROR("Cannot open memory mapped file %s.", setup_info.file_name);
		return ERROR_JTAG_INIT_FAILED;
	}

	mmap_regbase = (uint8_t *)mmap(NULL, setup_info.file_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev_mem_fd, 0);
	if (mmap_regbase == MAP_FAILED) {
		LOG_ERROR("Cannot allocate space for memory mapped file.");
		close(dev_mem_fd);
		return ERROR_JTAG_INIT_FAILED;
	}

	LOG_INFO("Picozed config success.");
	LOG_DEBUG("\n"
		"tdo_byte %d\n"
		"tdo_bit  %d\n"
		"tdi_byte %d\n"
		"tdi_bit  %d\n"
		"trst_byte %d\n"
		"trst_bit %d\n"
		"tms_byte %d\n"
		"tms_bit  %d\n"
		"tck_byte %d\n"
		"tck_bit  %d\n",
		setup_info.pins.name.tdo_byte,
		setup_info.pins.name.tdo_bit ,
		setup_info.pins.name.tdi_byte,
		setup_info.pins.name.tdi_bit ,
		setup_info.pins.name.trst_byte,
		setup_info.pins.name.trst_bit,
		setup_info.pins.name.tms_byte,
		setup_info.pins.name.tms_bit ,
		setup_info.pins.name.tck_byte,
		setup_info.pins.name.tck_bit );
	return ERROR_OK;
}

COMMAND_HANDLER(picozed_handle_set_pin_command)
{
	if (CMD_ARGC != 3)
	{
		LOG_ERROR("Invalid number of arguments.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	if(!mmap_regbase)
	{
		LOG_ERROR("Must call picozed_config before setting any pins.");
		return ERROR_FAIL;
	}

	int byte = atoi(CMD_ARGV[0]);
	if(byte < 0 || byte >= setup_info.file_size)
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

	LOG_INFO("Set byte %d, bit %d to %d.", byte, bit, value);
	return ERROR_OK;
}

static const struct command_registration picozed_command_handlers[] = 
{
	{
		.name = "picozed_config",
		.handler = &picozed_handle_config_command,
		.mode = COMMAND_CONFIG,
		.help = "Set memory map and pin configuration.\n"
			"Byte and bit positions must be base 10 integer and represent a byte offset from the beginning of the file and the bit position in that byte.\n"
			"For example, byte 1 and bit 7 is the most siginificant bit of the second byte of the memory mapped file.\n",
		.usage = "<memory mapped file name> <memory mapped file size> <TDO byte> <TDO bit> <TDI byte> <TDI bit> <TRST byte> <TRST bit> <TMS byte> <TMS bit> <TCK byte> <TCK bit>",
	},
	{
		.name = "picozed_set_pin",
		.handler = &picozed_handle_set_pin_command,
		.mode = COMMAND_ANY,
		.help = "Set arbitrary pin state.\n"
			"Byte value must be within the configured size.\n"
			"Byte and bit must be base 10 integers, but do not need to be one of the configued jtag pins.\n"
			"Value must be either 0 or 1.",
		.usage = "<byte> <bit> <value>",
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
