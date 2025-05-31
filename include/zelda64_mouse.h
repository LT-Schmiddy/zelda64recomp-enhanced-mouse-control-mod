#ifndef __RECOMP_MOUSE_H__
#define __RECOMP_MOUSE_H__

#include "modding.h"

 // @recomp mouse deltas export
RECOMP_IMPORT("*", void zelda64_get_mouse_deltas(float* x, float* y));
RECOMP_IMPORT("*", unsigned int zelda64_get_mouse_wheel_pos());
RECOMP_IMPORT("*", unsigned int zelda64_get_mouse_buttons());
RECOMP_IMPORT("*", unsigned int zelda64_get_mouse_button_mask());
RECOMP_IMPORT("*", void zelda64_set_mouse_button_mask(unsigned int mask));

#endif
