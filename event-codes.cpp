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

#include "event-codes.h"

#define BeginKey() EventName g_key_names[KEY_CNT] = {
#define BitKey(x) {x, #x},
#define BitKey2(x,y) {x, #x},
#define EndKey() };

#define BeginRel() EventName g_rel_names[REL_CNT] = {
#define BitRel(x) {x, #x},
#define EndRel() };

#define BeginAbs() EventName g_abs_names[ABS_CNT] = {
#define BitAbs(x) {x, #x},
#define EndAbs() };

#include "event-codes.inc"


