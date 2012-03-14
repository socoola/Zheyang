#ifndef _P_PLATFORM_H_
#define _P_PLATFORM_H_

typedef enum
{
	WIFI_MODULE_NONE = 0,
	WIFI_MODULE_RT3070	
} WIFI_MODULE;

bool p_init_platform();

//bool p_do_platform_handler();

#define p_reboot() system("reboot")

void p_feed_watchdog();

extern WIFI_MODULE g_wifi_module;
#define p_get_wifi_module()	g_wifi_module

#define UPGRADE_OK								0
#define UPGRADE_INVALID_FILE					-1
#define UPGRADE_FILE_SIZE_EXCEEDED				-2
#define UPGRADE_ERROR_CHECKSUM					-3
#define UPGRADE_FILENAME_SIZE_EXCEEDED			-4
#define UPGRADE_FILE_OPERATION_FAILED			-5
#define UPGRADE_FIRMWARE_MISMATCH				-6
#define UPGRADE_BUSY							-7

int p_upgrade_firmware(unsigned char * data, unsigned int len, bool anyway, const char * version_now);

int p_upgrade_htmls(unsigned char * data, unsigned int len);

bool p_mount_sd();

bool p_unmount_sd();

bool p_mount_smb(const char * svr, const char * folder, const char * user, const char * pwd);

bool p_unmount_smb();

bool p_i2c_process(unsigned char addr, bool stop_between, unsigned int read_num, unsigned char * read_data, unsigned int write_num, unsigned char * write_data);

#endif
