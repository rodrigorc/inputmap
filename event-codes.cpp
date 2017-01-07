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


