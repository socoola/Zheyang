/*
 * Copyright (C) 2004 Nathan Lutchansky <lutchann@litech.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#ifdef HAVE_ASM_TYPES_H
#include <asm/types.h>
#endif

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <inputs.h>
#include <conf_parse.h>
#if MACHINE_PC > 0
#include <ortp/ortp.h>
#endif
#include "include/libcomponent.h"
#include "include/list.h"
#include "include/av_type.h"
#include "include/c_audio.h"
#include "include/c_user.h"
#include "include/c_video.h"
#include "include/utility.h"

#define	INPUTTYPE_WEBCAM	1
#define INPUTTYPE_NTSC		2
#define INPUTTYPE_PAL		3

//#define MACHINE_PC      1
struct audio_spook_input {
	struct stream *output;
	struct frame_exchanger *ex;
	char device[256];
	int format;
	int bitrate;
	int width;
	int height;
	int fincr;
	int fbase;
	int inputport;
	int inputtype;
	int fps;
	int fd;
	void *bufaddr[2];
	int running;
    int localport;
    int nStream;
    struct event *ev;
#if MACHINE_PC > 0
    pthread_t thread;
    RtpSession *session;
    GET_AUDIO_CHUNK_CALLBACK pf;
#endif

};

static struct audio_spook_input *conf_array[4];

static struct audio_spook_input *get_conf(int id)
{
    return (id > 4) ? NULL : conf_array[id];
}

#if MACHINE_PC > 0
static int index_adjust[8] = {-1,-1,-1,-1,2,4,6,8};

static int step_table[89] = 
{
	7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
	50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,
	408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,
	2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,
	10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};

void adpcm_decode(unsigned char * raw, int len, unsigned char * decoded, int * pre_sample, int * index)
{
	int i;
	int code;
	int sb;
	int delta;
	short * pcm = (short *)decoded;
	len <<= 1;
	
	for (i = 0;i < len;i ++)
	{
		if (i & 0x01)
			code = raw[i >> 1] & 0x0f;	
		else
			code = raw[i >> 1] >> 4;	
		if ((code & 8) != 0) 
			sb = 1;
		else 
			sb = 0;
		code &= 7;	

		delta = (step_table[* index] * code) / 4 + step_table[* index] / 8;	
		if (sb) 
			delta = -delta;
		* pre_sample += delta;	
		if (* pre_sample > 32767)
			* pre_sample = 32767;
		else if (* pre_sample < -32768)
			* pre_sample = -32768;
		pcm[i] = * pre_sample;
		* index += index_adjust[code];
		if (* index < 0) 
			* index = 0;
		if (* index > 88) 
			* index = 88;
	}
}

bool c_request_audio(GET_AUDIO_CHUNK_CALLBACK callback)
{
    struct audio_spook_input *conf;

    conf = get_conf(0);
    conf->pf = callback;
}

bool c_end_audio(GET_AUDIO_CHUNK_CALLBACK callback)
{
    struct audio_spook_input *conf;

    conf = get_conf(0);
    conf->pf = NULL;
}

static void *capture_loop( void *d )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)d;
	int status;
    unsigned char buffer[38400];
    int have_more, err, stream_received=0;
    uint32_t ts=0;
    AUDIO_CHUNK *vf;

    do
    {
		have_more=1;
		while (have_more)
        {
			err=rtp_session_recv_with_ts(conf->session,buffer,sizeof(buffer),ts,&have_more);
			if (err>0) 
			{
                stream_received=1;
			}
			
			if ((stream_received) && (err>0)) 
            {
				if(conf->pf)
				{
                    vf = (AUDIO_CHUNK *)malloc(sizeof(AUDIO_CHUNK));
                    if(vf)
                    {
                        vf->data = (unsigned char *)malloc(err);
                        if(!vf->data)
                        {
                            free(vf);
                        }
                        else
                        {
                            switch(conf->format)
                            {
                                case FORMAT_PCM:
                                    vf->codec = PCM;
                                    break;

                                case FORMAT_ADPCM:
                                    vf->codec = ADPCM;
                                    break;

                                default:
                                    break;
                            }

                            memcpy(vf->data, buffer, err);
                            vf->len = err;
                            
                            conf->pf(vf);
                        }
                    }

                    
				}
			}
		}
		ts+=160;
		//ortp_message("Receiving packet.");
	}while(1);

	return NULL;
}
#endif

void get_audio_chunk_cb(AUDIO_CHUNK * vf)
{
    int sample, index;
    struct frame *f;
    struct audio_spook_input *conf;
    static int nAudioChunkSize = 0;
    static int skt = -1;
    struct iovec v[2];
    struct msghdr mh;
    static struct sockaddr_in stAddr;

    //printf("\r\nRcv audio chunk at:%u", get_tickcount() * 10);
    /*
    if(skt < 0)
    {        
        skt = socket(AF_INET, SOCK_DGRAM, 0);
        memset(&stAddr, 0, sizeof(stAddr));
        stAddr.sin_family = AF_INET;
        bind(skt, &stAddr, sizeof(stAddr));
        memset(&stAddr, 0, sizeof(stAddr));
        stAddr.sin_family = AF_INET;
        stAddr.sin_port = htons(6000);
        stAddr.sin_addr.s_addr = inet_addr("192.168.0.229");
        connect(skt, &stAddr, sizeof(stAddr));
    }    

    v[0].iov_base = &vf->tick;
    v[0].iov_len = 4;
    v[1].iov_base = vf->data;
    v[1].iov_len = vf->len;

    memset( &mh, 0, sizeof( mh ) );
    mh.msg_iov = v;
    mh.msg_iovlen = 2;
    sendmsg(skt, &mh, 0);
    */
    //free(vf->data);
    //free(vf);
    //return;
    
    conf = get_conf(0);
    if( ( f = get_next_frame( conf->ex, 0 ) ) )
	{
		f->length = vf->len;
        f->timestamp = vf->tick;
        switch(vf->codec)
        {
            case PCM:
                f->format = FORMAT_PCM;
                f->d = vf->data;
                free(vf);
                break;

            case ADPCM:
                if(conf->format == FORMAT_ADPCM)
                {
                    f->format = FORMAT_ADPCM;
                    f->d = vf->data;
                    free(vf);                    
                }
                else
                {
                    f->format = FORMAT_PCM;
                    vf->len -= 4;
                    f->d = malloc(vf->len * 4);
                    if(!f->d)
                    {
                        free(vf->data);
                        free(vf);
                        return;
                    }

                    sample = *(short*)vf->data;
                    index = *(short*)(vf->data + 2);
                    adpcm_decode(vf->data + 4, vf->len, f->d, &sample, &index);
                    if(nAudioChunkSize == 0)
                    {
                        nAudioChunkSize = vf->len;
                        spook_log( SL_INFO, "audio: Frame Length->%d", nAudioChunkSize);
                    }
                    
                    f->length = vf->len << 2;
                    {
                        int j;
                        unsigned char temp;
                        for(j = 0; j < f->length; j += 2)
                        {
                            temp = f->d[j];
                            f->d[j] = f->d[j+1];
                            f->d[j+1]=temp;
                            //*((short*)(f->d + j)) =
                            //    (f->d[j] << 8) + 
                            //    (f->d[j+1]);
                        }
                    }
                    free(vf->data);
                    free(vf);
                }
                
                break;

            default:
                spook_log( SL_ERR,
			"audio: audio format is not support" );
                free(vf->data);
                free(vf);
                return;
        }

        f->key = 1;
		deliver_frame( conf->ex, f );
		//printf("\r\nDeliver packet with length %d", vf->len);
	} 
    else
	{
		//spook_log( SL_WARN, "audio: dropping frame" );
		free(vf->data);
        free(vf);
	}
}

static void get_back_frame( struct frame *f, void *d )
{
	struct frame *newf;
	struct audio_spook_input *conf = (struct audio_spook_input *)d;

    newf = new_frame();
    if(newf)
    {
        exchange_frame( conf->ex, newf );
	    deliver_frame_to_stream( f, conf->output );
    }
    else
    {
		spook_log( SL_INFO, "Audio get_back_frame failed");
        unref_frame(f);
    }
}

#if MACHINE_PC > 0
static void ssrc_cb(RtpSession *session)
{
	printf("hey, the ssrc has changed !\n");
}
#endif

static int audio_setup( struct audio_spook_input *conf )
{
#if MACHINE_PC > 0
	// ??RTP??
/*
    conf->session=rtp_session_new(RTP_SESSION_RECVONLY);
    rtp_session_set_scheduling_mode(conf->session,1);
	rtp_session_set_blocking_mode(conf->session,1);
	rtp_session_set_local_addr(conf->session,"0.0.0.0", conf->localport);
	rtp_session_set_connected_mode(conf->session,TRUE);
	rtp_session_set_symmetric_rtp(conf->session,TRUE);
    rtp_session_enable_adaptive_jitter_compensation(conf->session,TRUE);
	rtp_session_set_jitter_compensation(conf->session,40);
	rtp_session_set_payload_type(conf->session,0);
	rtp_session_signal_connect(conf->session,"ssrc_changed",(RtpCallback)ssrc_cb,0);
	rtp_session_signal_connect(conf->session,"ssrc_changed",(RtpCallback)rtp_session_reset,0);
*/
#endif
	return 0;
}


static void get_framerate( struct stream *s, int *fincr, int *fbase )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)s->private;

	*fincr = conf->fincr;
	*fbase = conf->fbase;
}

void capture_audio( struct event_info *e, void *d )
{
    int err = 160;
    struct audio_spook_input *conf = (struct audio_spook_input *)d;
    AUDIO_CHUNK *vf = (AUDIO_CHUNK *)malloc(sizeof(AUDIO_CHUNK));
    if(vf)
    {
        vf->data = (unsigned char *)malloc(err);
        if(!vf->data)
        {
            free(vf);
        }
        else
        {
            vf->codec = PCM;
            //memcpy(vf->data, buffer, err);
            vf->len = err;
            
            //conf->pf(vf);
        }
    }
}


static void set_running( struct stream *s, int running )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)s->private;

	conf->running = running;
    if(running)
    {
        spook_log( SL_INFO, "Begin audio capture");
        c_request_audio(get_audio_chunk_cb, ADPCM);
        //conf->ev = add_timer_event(40, 0, capture_audio, conf);
        //set_event_enabled( conf->ev, 1 );
    }
    else
    {
        spook_log( SL_INFO, "End Audio capture");
        //remove_event(conf->ev);
        c_end_audio(get_audio_chunk_cb);
    }
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
    int i;
	struct audio_spook_input *conf;

	conf = (struct audio_spook_input *)malloc( sizeof( struct audio_spook_input ) );
	conf->output = NULL;
	conf->device[0] = 0;
	conf->format = FORMAT_RAW_UYVY;
	conf->bitrate = 0;
	conf->width = 0;
	conf->height = 0;
	conf->inputport = -1;
	conf->inputtype = 0;
	conf->fps = -1;
	conf->fd = -1;
	conf->running = 0;
    conf->localport = 0;
#if MACHINE_PC > 0
    conf->session = NULL;
#endif

    for(i = 0; i < 4; i++)
    {
        if(conf_array[i] == NULL)
        {
            conf_array[i] = conf;
            conf->nStream = i;
            return conf;
        }
    }

    spook_log( SL_ERR, "audio: too many channels" );
	return NULL;
}

static int end_block( void *d )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)d;
	int i;

	if( ! conf->output )
	{
		spook_log( SL_ERR, "rtpi: missing output stream name" );
		return -1;
	}
	if( conf->fps < 0 )
	{
		spook_log( SL_ERR,
			"v4l: framerate not specified for webcam" );
		return -1;
	} else if( conf->fps > 0 )
	{
		conf->fincr = 1;
		conf->fbase = conf->fps;
	} 
    else
    {
    	spook_log( SL_INFO, "v4l: must figure out framerate" );
        return -1;
    }
        
	if( audio_setup( conf ) < 0 ) return -1;
	conf->ex = new_exchanger( 25, get_back_frame, conf );
	for( i = 0; i < 25; ++i ) 
        exchange_frame( conf->ex, new_frame() );

#if MACHINE_PC > 0
	pthread_create( &conf->thread, NULL, capture_loop, conf );
#endif

	return 0;
}

static int set_device( int num_tokens, struct token *tokens, void *d )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)d;

	strcpy( conf->device, tokens[1].v.str );
	return 0;
}

static int set_output( int num_tokens, struct token *tokens, void *d )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)d;

	conf->output = new_stream( tokens[1].v.str, conf->format, conf );
	if( ! conf->output )
	{
		spook_log( SL_ERR, "audio: unable to create stream \"%s\"",
				tokens[1].v.str );
		return -1;
	}

    spook_log( SL_INFO, "Create audio stream:%s", tokens[1].v.str);
    conf->output->get_property = NULL;
	conf->output->get_framerate = get_framerate;
	conf->output->set_running = set_running;
	return 0;
}

static int set_format( int num_tokens, struct token *tokens, void *d )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)d;

	if( conf->output )
	{
		spook_log( SL_ERR, "rtpi: output format must be specified "
				"before output stream name" );
		return -1;
	}
    
    if( ! strcasecmp( tokens[1].v.str, "pcm" ) )
		conf->format = FORMAT_PCM;
	else if( ! strcasecmp( tokens[1].v.str, "adpcm" ) )
		conf->format = FORMAT_ADPCM;
	else
	{
		spook_log( SL_ERR, "audio: format \"%s\" is unsupported; try "
				"PCM, ADPCM", tokens[1].v.str );
		return -1;
	}

	return 0;
}

static int set_framerate_num( int num_tokens, struct token *tokens, void *d )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)d;

	if( conf->fps >= 0 )
	{
		spook_log( SL_ERR, "v4l: frame rate has already been set!" );
		return -1;
	}
	conf->fps = tokens[1].v.num;

	return 0;
}

static int set_framesize( int num_tokens, struct token *tokens, void *d )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)d;

	conf->width = tokens[1].v.num;
	conf->height = tokens[2].v.num;

	return 0;
}

static int set_port( int num_tokens, struct token *tokens, void *d )
{
	struct audio_spook_input *conf = (struct audio_spook_input *)d;

    conf->localport = tokens[1].v.num;

    return 0;
}

static struct statement config_statements[] = {
	/* directive name, process function, min args, max args, arg types */
	{ "device", set_device, 1, 1, { TOKEN_STR } },
	{ "output", set_output, 1, 1, { TOKEN_STR } },
	{ "format", set_format, 1, 1, { TOKEN_STR } },
    { "framerate", set_framerate_num, 1, 1, { TOKEN_NUM } },
    { "framesize", set_framesize, 2, 2, { TOKEN_NUM, TOKEN_NUM } },
    { "port", set_port, 1, 1, {TOKEN_NUM}},

	/* empty terminator -- do not remove */
	{ NULL, NULL, 0, 0, {} }
};

void audio_init(void)
{
#if MACHINE_PC > 0
/*
    ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(ORTP_ERROR);
	*/
#endif

	register_config_context( "input", "audio", start_block, end_block,
					config_statements );
}

int audio_deinit()
{
    int i;
    
    for(i = 0; i < sizeof(conf_array)/sizeof(struct audio_spook_input*); i++)
    {
        if(!conf_array[i])
        {
            continue;
        }
        
        if(conf_array[i]->output)
        {
            del_stream(conf_array[i]->output);
            conf_array[i]->output = NULL;
        }

        if(conf_array[i]->ex)
        {
            del_exchanger(conf_array[i]->ex);
            conf_array[i]->ex = NULL;
        }

        free(conf_array[i]);
    }
}




