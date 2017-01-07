#ifndef EVENT_CODES_H_INCLUDED
#define EVENT_CODES_H_INCLUDED

#include <linux/input-event-codes.h>

struct EventName
{
    int id;
    const char *name;
};

extern EventName g_key_names[KEY_CNT];
extern EventName g_rel_names[REL_CNT];
extern EventName g_abs_names[ABS_CNT];

#endif /* EVENT-CODES_H_INCLUDED */
