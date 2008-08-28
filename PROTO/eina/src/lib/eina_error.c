/* EINA - EFL data type library
 * Copyright (C) 2007-2008 Jorge Luis Zapata Muga, Cedric Bail
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library;
 * if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "eina_error.h"
#include "eina_list.h"
#include "eina_private.h"

/* TODO
 * + printing errors to stdout or stderr can be implemented
 * using a queue, useful for multiple threads printing
 * + add a wapper for assert?
 * + add common error numbers, messages
 * + add a calltrace of erros, not only store the last error but a list of them
 * and also store the function that set it
 */

/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

static int _init_count = 0;
static Eina_List *_error_list;
static Eina_Error _err;
static Eina_Error_Print_Cb _print_cb = eina_error_print_cb_stdout;
static void *_print_cb_data = NULL;

#define RED     "\033[31;1m"
#define GREEN   "\033[32;1m"
#define YELLOW  "\033[33;1m"
#define WHITE   "\033[37;1m"
#define NOTHING "\033[0m"

EAPI Eina_Error EINA_ERROR_OUT_OF_MEMORY = 0;

#ifdef DEBUG
static Eina_Error_Level _error_level = EINA_ERROR_LEVEL_DBG;
#else
static Eina_Error_Level _error_level = EINA_ERROR_LEVEL_ERR;
#endif

static char *_colors[EINA_ERROR_LEVELS] = {
	[EINA_ERROR_LEVEL_ERR] = RED,
	[EINA_ERROR_LEVEL_WARN] = YELLOW,
	[EINA_ERROR_LEVEL_INFO] = NOTHING,
	[EINA_ERROR_LEVEL_DBG] = GREEN,
};


/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/

/**
 *
 */
EAPI int eina_error_init(void)
{
	if (!_init_count)
	{
		char *level;
		/* TODO register the eina's basic errors */
		/* TODO load the environment variable for getting the log level */
		if ((level = getenv("EINA_ERROR_LEVEL")))
		{
			_error_level = atoi(level);
		}
		EINA_ERROR_OUT_OF_MEMORY = eina_error_register("Out of memory");
	}
	/* get all the modules */
	return ++_init_count;
}
/**
 *
 */
EAPI int eina_error_shutdown(void)
{
	_init_count--;
	if (!_init_count)
	{
		/* remove the error strings */
		while (_error_list)
		{
			free(eina_list_data(_error_list));
			_error_list = eina_list_free(_error_list);
		}
	}
	return _init_count;
}

/**
 * Register a new error type
 * @param msg The description of the error
 * @return The unique number identifier for this error
 */
EAPI Eina_Error eina_error_register(const char *msg)
{
	_error_list = eina_list_append(_error_list, strdup(msg));

	return eina_list_count(_error_list);
}
/**
 *
 */
EAPI Eina_Error eina_error_get(void)
{
	return _err;
}
/**
 *
 */
EAPI void eina_error_set(Eina_Error err)
{
	_err = err;
}
/**
 * Given an error number return the description of it
 * @param error The error number
 * @return The description of the error
 */
EAPI const char * eina_error_msg_get(int error)
{
	return eina_list_nth(_error_list, error - 1);
}
/**
 *
 */
EAPI void eina_error_print(Eina_Error_Level level, const char *file,
		const char *fnc, int line, const char *fmt, ...)
{
	va_list args;

	if (level > _error_level)
		return;

	va_start(args, fmt);
	_print_cb(level, file, fnc, line, fmt, _print_cb_data, args);
	va_end(args);
}
/**
 *
 */
EAPI void eina_error_print_cb_stdout(Eina_Error_Level level, const char *file,
		const char *fnc, int line, const char *fmt, __UNUSED__ void *data,
		va_list args)
{
	printf("%s", _colors[level]);
	printf("[%s:%d] %s() ", file, line, fnc);
	printf("%s", _colors[EINA_ERROR_LEVEL_INFO]);
	vprintf(fmt, args);
}
/**
 *
 */
EAPI void eina_error_print_cb_file(__UNUSED__ Eina_Error_Level level, const char *file,
		const char *fnc, int line, const char *fmt, void *data,
		va_list args)
{
	FILE *f = data;

	fprintf(f, "[%s:%d] %s() ", file, line, fnc);
	vfprintf(f, fmt, args);
}
/**
 *
 */
EAPI void eina_error_print_cb_set(Eina_Error_Print_Cb cb, void *data)
{
	_print_cb = cb;
	_print_cb_data = data;
}
/**
 *
 */
EAPI void eina_error_log_level_set(Eina_Error_Level level)
{
	_error_level = level;
}
