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
	};
};

struct jtag_setup
{
	char* file_name;
	int file_size;
	struct jtag_pins pins;
};

/* interface variables
 */
static int dev_mem_fd = -1;
static volatile char * mmap_regbase = NULL;
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
	return ((mmap_regbase[setup_info.tdo_byte] >> tdo_bit) & 1) ? B_HIGH : BB_LOW;
}

static int picozed_write(int tck, int tms, int tdi)
{
	if (tck)
		mmap_regbase[setup_info.tck_byte] |= 1 << setup_info.tck_bit;
	else
		mmap_regbase[setup_info.tck_byte] &= ~(1 << setup_info.tck_bit);

	if (tck)
		mmap_regbase[setup_info.tms_byte] |= 1 << setup_info.tms_bit;
	else
		mmap_regbase[setup_info.tms_byte] &= ~(1 << setup_info.tms_bit);

	if (tck)
		mmap_regbase[setup_info.tdi_byte] |= 1 << setup_info.tdi_bit;
	else
		mmap_regbase[setup_info.tdi_byte] &= ~(1 << setup_info.tdi_bit);

	return ERROR_OK;
}

/* (1) assert or (0) deassert reset lines */
static int picozed_reset(int trst, int srst)
{
	if (trst == 0)
		mmap_regbase[setup_info.trst_byte] |= 1 << setup_info.trst_bit;
	else if (trst == 1)
		mmap_regbase[setup_info.trst_byte] &= ~(1 << setup_info.trst_bit);

	return ERROR_OK;
}

static int picozed_init(void)
{
	bitbang_interface = &picozed_bitbang;

	dev_mem_fd = open(setup_info.file_name, O_RDWR);
	if (dev_mem_fd < 0) {
		LOG_ERROR("Cannot open memory mapped file %s,", setup_info.file_name);
		return ERROR_JTAG_INIT_FAILED;
	}

	mmap_regbase = (char *)mmap(NULL, setup_info.file_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev_mem_fd, 0);
	if (mmap_regbase == MAP_FAILED) {
		LOG_ERROR("Cannot allocate space for memory mapped file.");
		close(dev_mem_fd);
		return ERROR_JTAG_INIT_FAILED;
	}


	//JTAG_REG |= JTAG_SW; // PSOC
	picozed_write(0, 1, 0);
	picozed_reset(1, 1);

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
		free(setup_info.file_name)
		setup_info.file_name = NULL;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(picozed_handle_config_command)
{
	if (CMD_ARGC != 12)
	{
		LOG_ERROR("Invalid number of arguments");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	int fn_len = strlen(CMD_ARGV[0]);
	jtag_setup.file_name = malloc(fn_len);
	if(!jtag_setup.file_name)
		return ERROR_FAIL;
	memcpy(jtag_setup.file_name, CMD_ARGV[0], fn_len);

	jtag_setup.file_size = atoi(CMD_ARGV[1]);

	for(int i = 0; i < 10; i++)
		jtag_setup.pins.arr[i] = atoi(CMD_ARGV[i + 2]);
}

static const struct command_registration picozed_exec_command_handlers[] = {
	{
		.name = "config",
		.handler = &picozed_handle_config_command,
		.mode = COMMAND_CONFIG,
		.help = "Set memory map and pin configuration.\n"
			"Byte and bit positions must be base 10 integer and represent a byte offset from the beginning of the file and the bit position in that byte.\n"
			"For example, byte 1 and bit 7 is the most siginificant bit of the second byte of the memory mapped file.\n",
		.usage = "<memory mapped file name> <memory mapped file size> <TDO byte> <TDO bit> <TDI byte> <TDI bit> <TRST byte> <TRST bit> <TMS byte> <TMS bit> <TCK byte> <TCK bit>",
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
