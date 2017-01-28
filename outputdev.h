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
