/***************************************************************************
 *   Copyright (C) 2009 Zachary T Welch <zw@superlucidity.net>             *
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
#include "imp.h"

extern const struct flash_driver psoc5lp_flash;
extern const struct flash_driver psoc5lp_eeprom_flash;
extern const struct flash_driver psoc5lp_nvl_flash;

/**
 * The list of built-in flash drivers.
 * @todo Make this dynamically extendable with loadable modules.
 */
static const struct flash_driver * const flash_drivers[] = {
	&psoc5lp_flash,
	&psoc5lp_eeprom_flash,
	&psoc5lp_nvl_flash,
	NULL,
};

const struct flash_driver *flash_driver_find_by_name(const char *name)
{
	for (unsigned i = 0; flash_drivers[i]; i++) {
		if (strcmp(name, flash_drivers[i]->name) == 0)
			return flash_drivers[i];
	}
	return NULL;
}
