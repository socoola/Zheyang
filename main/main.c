#include "include/libcomponent.h"
#include "t_video.h"
#include "t_audio.h"
#include "c_udp_server.h"
#include "enet/enet.h"
#include "log.h"
#include "config.h"

extern unsigned int g_ulLocalAddr;
extern int g_bForceRtcp;
extern int g_bMulticastMode;
extern unsigned int g_ulChkTv;

int main(int argc, char **argv)
{
    int i;
	char *config_file = WORK_DICTORY"//spook.conf";

	while( ( i = getopt( argc, argv, "FMc:l:t:" ) ) != -1 )
	{
		switch( i )
		{
		case 'c':
			config_file = optarg;
			break;
            
        case 'F':
            g_bForceRtcp = 1;
            break;

        case 'M':
            g_bMulticastMode = 1;
            break;
            
        case 'l':
            g_ulLocalAddr = inet_addr(optarg);
            break;

        case 't':
            g_ulChkTv = atoi(optarg);
            break;
            
		case '?':
		case ':':
			return 1;
			break;
		}
	}
    
	ignore_sigpipe();    

	if (! p_init_platform())
		return -1;
    
	if (! c_probe_sensor())
		return -1;
	c_init_params();
	
	if (! t_create_video_thread())
		return -1;
	if (! t_create_audio_thread())
		return -1;    

    if(argc > 1)
    {
        //printf("\r\n%s", argv[1]);
        sleep(5);
    }
    else
    {
        sleep(4);
    }

    //spook_log_init( SL_VERBOSE );

    t_create_speak_thread();
    
    t_create_wireless_check_thread();
    /*
    while(1)
    {
        int a = 0;
        printf("\r\nPlease input type:0--connect 1---show status\r\n");
        scanf("%d", &a);
        switch(a)
        {
            case 0:
                connect2ssid("zycamera");
                break;
            case 1:
                iwconfig("iwconfig eth1");
                break;

            case 2:
                iwpriv("iwpriv eth1");
                break;

            case 3:
                iwpriv("iwpriv eth1 set NetworkType=Infra");
                break;

            case 4:
                iwpriv("iwpriv eth1 set AuthMode=Open");
                break;

            case 5:
                iwpriv("iwpriv eth1 set EncrypType=None");
                break;

            case 6:
                iwpriv("iwpriv eth1 set SSID=");
                break;

            case 7:
                iwpriv("iwpriv eth1 set SSID=zycamera");
                break;
        }
    }
    */
	//c_init_udp_server(config_file);
	//t_create_rtsp_thread();    
    //t_create_speak_thread();
    

    c_init_rtsp_server();
	while (1)
	{
		//c_do_udp_server_handler();	
		c_do_rtsp_server_handler();
		//sleep(1);
	}
	
	//c_deinit_udp_server();
	

	return 0;
}

#if 0
int main()
{
    ENetAddress address;
    ENetHost * server;
    ENetEvent event;
    ENetPacket * packet;

    if (enet_initialize () != 0)
    {
        printf("An error occurred while initializing ENet.\n");
        return 0;
    }

    printf("success of initializing ENet.\n");

    /* Bind the server to the default localhost.     */
    /* A specific host address can be specified by   */
    /* enet_address_set_host (& address, "x.x.x.x"); */

    address.host = ENET_HOST_ANY;
    /* Bind the server to port 1234. */
    address.port = 12345;

    server = enet_host_create (& address /* the address to bind the server host to */, 
                                 1      /* allow up to 32 clients and/or outgoing connections */,
                                  2      /* allow up to 2 channels to be used, 0 and 1 */,
                                  0      /* assume any amount of incoming bandwidth */,
                                  0      /* assume any amount of outgoing bandwidth */);
    if (server == NULL)
    {
        printf("An error occurred while trying to create an ENet server host.\n");
        enet_deinitialize();
        return 0;
    }

    printf("Successed to create an ENet server host.\n");
    while (1)
    {
        /* Wait up to 1000 milliseconds for an event. */
        if (enet_host_service (server, & event, 1000) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                printf("A new client connected from %x:%u.\n", 
                        event.peer -> address.host,
                        event.peer -> address.port);

                /* Store any relevant client information here. */
                event.peer -> data = 0;

                break;

            case ENET_EVENT_TYPE_RECEIVE:
                printf("A packet of length %u was received on channel %u.\n",
                        event.packet -> dataLength,
                        event.channelID);

                if((event.peer->data == 0) && (event.channelID == 0))
                {
                    unsigned char szReply[32];
                    
                    szReply[0] = 0x01;
                    szReply[1] = 0x04;                    
                    packet = enet_packet_create(szReply, 2, ENET_PACKET_FLAG_RELIABLE);
                    if(packet)
                    {
                        event.peer->data = 1;
                        enet_peer_send (event.peer, 0, packet);
                        //c_request_speak(200);
                    }
                    printf("Receive Login Request!");
                }
                else if(event.channelID == 1)
                {
                    AUDIO_CHUNK *pChunk;

                    pChunk = (AUDIO_CHUNK *)event.packet->data;
                    if(pChunk->len == (event.packet->dataLength - sizeof(AUDIO_CHUNK) + sizeof(struct list_head)))
                    {
                        pChunk = (AUDIO_CHUNK *)malloc(sizeof(AUDIO_CHUNK));
                        if(pChunk)
                        {
                            memcpy(pChunk, event.packet->data, sizeof(AUDIO_CHUNK));     
                            pChunk->data = malloc(pChunk->len);
                            if(pChunk->data)
                            {
                                memcpy(pChunk->data, 
                                event.packet->data + sizeof(AUDIO_CHUNK) - sizeof(struct list_head), 
                                pChunk->len);
                                INIT_LIST_HEAD(&pChunk->list);
                                //c_speak(pChunk);
                                free(pChunk);
                            }
                            else
                            {
                                free(pChunk);
                            }
                        }
                    }                    
                }
                else
                {
                    printf("Receive Unknown Request!");
                    enet_peer_reset (event.peer);
                    //c_end_speak();
                }

                /* Clean up the packet now that we're done using it. */                
                enet_packet_destroy (event.packet);                
                break;
               
            case ENET_EVENT_TYPE_DISCONNECT:
                //printf("%s disconected.\n", event.peer -> data);

                /* Reset the peer's client information. */
                event.peer -> data = NULL;
                //c_end_speak();
                break;
            }
        }
    }

    enet_host_destroy(server);
    printf("speak quited\n");
    enet_deinitialize();

    return 0;
}
#endif
