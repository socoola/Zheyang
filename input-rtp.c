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
#ifdef HAVE_LINUX_COMPILER_H
#include <linux/compiler.h>
#endif
#include <linux/videodev2.h>
#ifdef HAVE_GO7007_H
#include <linux/go7007.h>
#endif

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <inputs.h>
#include <conf_parse.h>
#include <ortp/ortp.h>

#define	INPUTTYPE_WEBCAM	1
#define INPUTTYPE_NTSC		2
#define INPUTTYPE_PAL		3

struct rtp_spook_input {
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
	pthread_t thread;
	int running;
    int localport;
    RtpSession *session;
};

static void *capture_loop( void *d )
{
	struct rtp_spook_input *conf = (struct rtp_spook_input *)d;
	struct frame *f;
	int status;
    unsigned char buffer[38400];
    int have_more, err, stream_received=0;
    uint32_t ts=0;

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
				if( ( f = get_next_frame( conf->ex, 0 ) ) )
        		{
        			f->length = err;
        			f->format = conf->format;
        			f->width = conf->width;
        			f->height = conf->height;
        			f->key = 1;

        			memcpy( f->d, buffer, err);

        			deliver_frame( conf->ex, f );
					printf("\r\nDeliver packet with length %d", err);
        		} 
                else
        		{
        			spook_log( SL_WARN, "video: dropping frame" );
        		}
			}
		}
		ts+=160;
		//ortp_message("Receiving packet.");
	}while(1);

	return NULL;
}

static void get_back_frame1( struct frame *f, void *d )
{
	struct frame *newf;
	struct rtp_spook_input *conf = (struct rtp_spook_input *)d;

    newf = new_frame();
    if(newf)
    {
        exchange_frame( conf->ex, newf );
	    deliver_frame_to_stream( f, conf->output );
    }
    else
    {
		//printf("\r\ndrop frame!");
        unref_frame(f);
    }
}

static void ssrc_cb(RtpSession *session)
{
	printf("hey, the ssrc has changed !\n");
}

static int rtp_setup( struct rtp_spook_input *conf )
{
	// ??RTP??
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

	return 0;
}


static void get_framerate( struct stream *s, int *fincr, int *fbase )
{
	struct rtp_spook_input *conf = (struct rtp_spook_input *)s->private;

	*fincr = conf->fincr;
	*fbase = conf->fbase;
}

static void set_running( struct stream *s, int running )
{
	struct rtp_spook_input *conf = (struct rtp_spook_input *)s->private;

	conf->running = running;
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct rtp_spook_input *conf;

	conf = (struct rtp_spook_input *)malloc( sizeof( struct rtp_spook_input ) );
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
    conf->session = NULL;

	return conf;
}

static int end_block( void *d )
{
	struct rtp_spook_input *conf = (struct rtp_spook_input *)d;
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
        
	if( rtp_setup( conf ) < 0 ) return -1;
	conf->ex = new_exchanger( conf->fps, get_back_frame1, conf );
	for( i = 0; i < conf->fps; ++i ) 
        exchange_frame( conf->ex, new_frame() );
	pthread_create( &conf->thread, NULL, capture_loop, conf );

	return 0;
}

static int set_device( int num_tokens, struct token *tokens, void *d )
{
	struct rtp_spook_input *conf = (struct rtp_spook_input *)d;

	strcpy( conf->device, tokens[1].v.str );
	return 0;
}

static int set_output( int num_tokens, struct token *tokens, void *d )
{
	struct rtp_spook_input *conf = (struct rtp_spook_input *)d;

	conf->output = new_stream( tokens[1].v.str, conf->format, conf );
	if( ! conf->output )
	{
		spook_log( SL_ERR, "rtpi: unable to create stream \"%s\"",
				tokens[1].v.str );
		return -1;
	}
	conf->output->get_framerate = get_framerate;
	conf->output->set_running = set_running;
	return 0;
}

static int set_format( int num_tokens, struct token *tokens, void *d )
{
	struct rtp_spook_input *conf = (struct rtp_spook_input *)d;

	if( conf->output )
	{
		spook_log( SL_ERR, "rtpi: output format must be specified "
				"before output stream name" );
		return -1;
	}
	if( ! strcasecmp( tokens[1].v.str, "pcm" ) )
		conf->format = FORMAT_PCM;
	else if( ! strcasecmp( tokens[1].v.str, "mpeg4" ) )
		conf->format = FORMAT_MPEG4;
	else if( ! strcasecmp( tokens[1].v.str, "mjpeg" ) )
		conf->format = FORMAT_JPEG;
	else
	{
		spook_log( SL_ERR, "rtpi: format \"%s\" is unsupported; try "
				"PCM, MJPEG, or MPEG4", tokens[1].v.str );
		return -1;
	}

	return 0;
}

static int set_framerate_num( int num_tokens, struct token *tokens, void *d )
{
	struct rtp_spook_input *conf = (struct rtp_spook_input *)d;

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
	struct rtp_spook_input *conf = (struct rtp_spook_input *)d;

	conf->width = tokens[1].v.num;
	conf->height = tokens[2].v.num;

	return 0;
}

static int set_port( int num_tokens, struct token *tokens, void *d )
{
	struct rtp_spook_input *conf = (struct rtp_spook_input *)d;

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

void rtp_init(void)
{
    ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(ORTP_ERROR);
    
	register_config_context( "input", "rtp", start_block, end_block,
					config_statements );
}




