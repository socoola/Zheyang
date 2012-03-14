#ifndef _C_SPEAK_H_
#define _C_SPEAK_H_

bool c_init_speak_server(int port);
bool c_do_speak_server_handler();
void c_deinit_speak_server();

#endif