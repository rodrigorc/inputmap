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

#include "steam/fd.h"
#include "inifile.h"
#include "inputdev.h"
#include "devinput-parser.h"

struct FFEffect
{
    std::weak_ptr<InputDevice> device;
    int input_id;
};

class OutputDevice : public IPollable
{
public:
    OutputDevice(const IniSection &ini, IInputByName &inputFinder);
    void sync();

    virtual int fd() override { return m_fd.get(); }
    virtual PollResult on_poll(int event) override;

private:
    FD m_fd;
    std::vector<std::pair<int, std::unique_ptr<ValueExpr>>> m_rel;
    std::vector<std::pair<int, std::unique_ptr<ValueExpr>>> m_key;
    std::vector<std::pair<int, std::unique_ptr<ValueExpr>>> m_abs;
    std::vector<std::pair<int, std::unique_ptr<ValueRef>>> m_ff;

    ValueRef *get_ff(int id);
    void write_value(int type, int code, int value);

    std::vector<FFEffect> m_effects;
};

#endif /* OUTPUTDEV_H_INCLUDED */
