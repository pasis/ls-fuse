/* log.h
 * ls-fuse - ls -lR output mounter
 *
 * Copyright (C) 2013 Dmitry Podgorny <pasis.ua@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LS_FUSE_LOG_H
#define LS_FUSE_LOG_H

#define DEBUG_TAG "[D] "
#define ERROR_TAG "[E] "

#define logging_func(...) printf(__VA_ARGS__)

#ifdef DEBUG
#define LOGE(fmt, ...) logging_func(ERROR_TAG fmt "\n", ##__VA_ARGS__)
#define LOGD(fmt, ...) \
	logging_func(DEBUG_TAG "%s(): " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define LOGE(fmt, ...) logging_func(ERROR_TAG fmt "\n", ##__VA_ARGS__)
#define LOGD(fmt, ...) do {} while (0)
#endif

#endif /* LS_FUSE_LOG_H */
