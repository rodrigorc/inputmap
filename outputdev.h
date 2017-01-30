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

#ifndef OUTPUTDEV_H_INCLUDED
#define OUTPUTDEV_H_INCLUDED

#include "fd.h"
#include "inifile.h"
#include "inputdev.h"
#include "devinput-parser.h"

class OutputDevice
{
public:
    OutputDevice(const IniSection &ini, IInputByName &inputFinder);
    void sync();
private:
    FD m_fd;
    std::unique_ptr<ValueExpr> m_rel[REL_CNT];
    std::unique_ptr<ValueExpr> m_key[KEY_CNT];
    std::unique_ptr<ValueExpr> m_abs[ABS_CNT];

    void write_value(int type, int code, int value);
};

#endif /* OUTPUTDEV_H_INCLUDED */
