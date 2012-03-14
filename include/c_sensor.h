#ifndef _C_SENSOR_H_
#define _C_SENSOR_H_

extern SENSOR g_sensor;
extern unsigned int g_resolution_cap;
extern unsigned int g_camera_cap;

bool c_probe_sensor();

bool c_sensor_control(int command, int value);

#endif
