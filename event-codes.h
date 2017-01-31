/*

Copyright 2017, Rodrigo Rivas Costa <rodrigorivascosta@gmail.com>

This file is part of inputmap.

inputmap is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

inputmap is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with inputmap.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef EVENT_CODES_H_INCLUDED
#define EVENT_CODES_H_INCLUDED

#include <linux/input.h>
#include <linux/input-event-codes.h>

struct EventName
{
    int id;
    const char *name;
};

extern EventName g_key_names[KEY_CNT];
extern EventName g_rel_names[REL_CNT];
extern EventName g_abs_names[ABS_CNT];
extern EventName g_ff_names[FF_CNT];

#endif /* EVENT-CODES_H_INCLUDED */
