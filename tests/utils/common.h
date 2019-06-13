/*
 * common.h
 *
 * Lib BabelTrace - Common function to all tests
 *
 * Copyright 2012 - Yannick Brosseau <yannick.brosseau@gmail.com>
 * Copyright 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _TESTS_COMMON_H
#define _TESTS_COMMON_H

struct bt_context;

void recursive_rmdir(const char *path);

#endif /* _TESTS_COMMON_H */
