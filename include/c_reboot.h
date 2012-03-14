#ifndef _C_REBOOT_H_
#define _C_REBOOT_H_

extern bool g_reboot;

static inline void c_reboot()
{
	g_reboot = true;
}

#define c_is_rebooting()				g_reboot

#endif
