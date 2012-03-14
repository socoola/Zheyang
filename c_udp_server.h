#ifndef _C_UDP_SERVER_H_
#define _C_UDP_SERVER_H_

bool c_init_udp_server(const char *config_file);

void c_do_udp_server_handler();

void c_deinit_udp_server();
#endif
