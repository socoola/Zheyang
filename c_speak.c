#include "include/libcomponent.h"
#include "enet/enet.h"
#include <log.h>

#define TYPE_LOGIN		0x01
#define TYPE_REQ_SPEAK	0x02
#define TYPE_LOGOUT		0x03
#define TYPE_ACK		0x04
#define TYPE_REJ		0x05

#define MAX_CHN     5
#define MAX_CONN    5
ENetPeer *g_peers[MAX_CHN][MAX_CONN];
unsigned int g_nPeers[MAX_CHN];

static ENetAddress address;
static ENetHost * server;

bool c_init_speak_server(int port)
{
    int i, j;
    
    
    for(i = 0; i < MAX_CHN; i++)
    {
        for(j = 0; j < MAX_CONN; j++)
        {
            g_peers[i][j] = NULL;
        }
    }

    if (enet_initialize () != 0)
    {
        spook_log(SL_ERR, "An error occurred while initializing ENet.\n");
        return false;
    }

    spook_log(SL_INFO, "success of initializing ENet.\n");

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
        spook_log(SL_ERR, 
                 "An error occurred while trying to create an ENet server host.\n");
        enet_deinitialize();
        return false;
    }

    spook_log(SL_INFO, 
                 "Successed to create an ENet server host.\n");

    return true;
}

bool c_do_speak_server_handler()
{
    int i;
    ENetEvent event;
    ENetPacket * packet;
    
    /* Wait up to 1000 milliseconds for an event. */
    if (enet_host_service (server, & event, 10) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            spook_log(SL_INFO, "A new client connected from %x:%u.\n", 
                    event.peer -> address.host,
                    event.peer -> address.port);

            /* Store any relevant client information here. */
            event.peer -> data = 0;                

            break;

        case ENET_EVENT_TYPE_RECEIVE:
            /*
            spook_log(SL_INFO,
                "A packet of length %u was received on channel %u",
                    event.packet->dataLength,
                    event.channelID);
              */ 

            if((event.peer->data == 0) && (event.channelID == 0))
            {
                unsigned char szReply[32];
                
                szReply[0] = 0x01;
                szReply[1] = TYPE_ACK;                    
                packet = enet_packet_create(szReply, 2, ENET_PACKET_FLAG_RELIABLE);
                if(packet)
                {
                    for(i = 0; i < MAX_CONN; i++)
                    {
                        if(!g_peers[1][i])
                        {
                            g_peers[1][i] = event.peer;
                            break;
                        }
                    }

                    if(i == MAX_CONN)
                    {
                        enet_packet_destroy(packet);
                    }
                    else
                    {
                        if(enet_peer_send (event.peer, 0, packet) < 0)
                        {
                            g_peers[1][i] = NULL;
                            enet_packet_destroy(packet);
                        }
                        else
                        {
                            // c_request_video(get_video1_frame_cb,1);
                            if(!c_request_speak(50))
                            {
                                spook_log(SL_ERR, "call c_request_speak error");
                            }

                            event.peer->data = 1;
                            g_nPeers[1]++;
                            spook_log(SL_INFO, "Receive Login Request!");
                        }
                    }                        
                }
            }
            else if(event.channelID == 1)
            {
                AUDIO_CHUNK *pChunk;

                //@TODO: enum of AUDIO_CHUNK' size is diff on diff platform
                pChunk = (AUDIO_CHUNK *)event.packet->data;
                if(pChunk->len == (event.packet->dataLength - sizeof(AUDIO_CHUNK) + sizeof(struct list_head)))
                {
                    pChunk = (AUDIO_CHUNK *)malloc(sizeof(AUDIO_CHUNK));
                    if(pChunk)
                    {
                        // copy the header
                        memcpy(pChunk, event.packet->data, sizeof(AUDIO_CHUNK));     
                        pChunk->data = malloc(pChunk->len + 4);
                        if(pChunk->data)
                        {
                            memcpy(pChunk->data + 4, 
                            event.packet->data + sizeof(AUDIO_CHUNK)-sizeof(struct list_head), 
                            pChunk->len);
                            pChunk->len += 4;
                            *((short*)(pChunk->data)) = pChunk->sample;
                            *((short*)(pChunk->data + 2)) = pChunk->index;
                            INIT_LIST_HEAD(&pChunk->list);
                            
                            if(!c_speak(pChunk))
                            {
                                spook_log(SL_ERR, "call c_speak error:len %d", pChunk->len);
                            }
                            
                            //get_audio_chunk_cb(pChunk);
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
                spook_log(SL_WARN, "Receive Unknown Request!");
                enet_peer_reset (event.peer);                    
                if(!c_end_speak())
                {
                    spook_log(SL_ERR, "call c_end_speak error");
                }
            }

            /* Clean up the packet now that we're done using it. */
            enet_packet_destroy (event.packet);
            
            break;
           
        case ENET_EVENT_TYPE_DISCONNECT:
            spook_log(SL_INFO, "%d disconected.\n", event.peer -> data);

            /* Reset the peer's client information. */
            event.peer -> data = NULL;
            for(i = 0; i < MAX_CONN; i++)
            {
                if(g_peers[1][i] == event.peer)
                {
                    g_peers[1][i] = NULL;
                    g_nPeers[1]--;
                }
            }
            c_end_speak();
            //c_end_video(get_video1_frame_cb,1);
        }
    }

    return true;
}

void c_deinit_speak_server()
{
    spook_log(SL_INFO, "speak quited\n");
    enet_host_destroy(server);	
    enet_deinitialize();
}


