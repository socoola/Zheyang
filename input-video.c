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
#if MACHINE_PC > 0
#include <ortp/ortp.h>
#endif
#include "include/libcomponent.h"
#include "include/list.h"
#include "include/av_type.h"
#include "include/c_audio.h"
#include "include/c_user.h"
#include "include/c_video.h"
//#include "include/utility.h"

#define	INPUTTYPE_WEBCAM	1
#define INPUTTYPE_NTSC		2
#define INPUTTYPE_PAL		3

struct video_spook_input {
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
    MSG_CONTEXT msgQ;
#if MACHINE_PC > 0
    pthread_t thread;
    RtpSession *session;
    GET_VIDEO_FRAME_CALLBACK pf;
#endif

};

static struct video_spook_input *conf_array[4];

static struct video_spook_input *get_conf(int id)
{
    return (id >= 4) ? NULL : conf_array[id];
}

#if MACHINE_PC > 0
bool c_request_video(GET_VIDEO_FRAME_CALLBACK callback, unsigned char stream)
{
    struct video_spook_input *conf;

    conf = get_conf(stream);
    conf->pf = callback;
}

bool c_end_video(GET_VIDEO_FRAME_CALLBACK callback, unsigned char stream)
{
    struct video_spook_input *conf;

    conf = get_conf(stream);
    conf->pf = NULL;
}

static void *capture_loop( void *d )
{
	struct video_spook_input *conf = (struct video_spook_input *)d;
	int status;
    unsigned char buffer[38400];
    int have_more, err, stream_received=0;
    uint32_t ts=0;
    VIDEO_FRAME *vf;
/*
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
                    vf = (VIDEO_FRAME *)malloc(sizeof(VIDEO_FRAME));
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
                                case FORMAT_MPEG4:
                                    vf->codec = MP4S;
                                    break;

                                case FORMAT_JPEG:
                                    vf->codec = MJPG;
                                    break;

                                default:
                                    break;
                            }

                            vf->resolution = QCIF;
                            vf->stream = conf->nStream;
                            memcpy(vf->data, buffer, err);
                            vf->len = err;
                            vf->keyframe = 1;
                            
                            conf->pf(vf);
                        }
                    }

                    
				}
			}
		}
		ts+=160;
		//ortp_message("Receiving packet.");
	}while(1);
*/
    while(1)
    {
        AVFormatContext *pFormatCtx;
        int             i, videoStream;
        AVCodecContext  *pCodecCtx;
        AVCodec         *pCodec;
        AVFrame         *pFrame; 
        AVFrame         *pFrameRGB;
        AVPacket        packet;
        int             frameFinished;
        int             numBytes;
        uint8_t         *buffer;

        if(argc < 2) {
        printf("Please provide a movie file\n");
        return -1;
        }
        // Register all formats and codecs
        av_register_all();

        // Open video file
        if(av_open_input_file(&pFormatCtx, argv[1], NULL, 0, NULL)!=0)
        return -1; // Couldn't open file

        // Retrieve stream information
        if(av_find_stream_info(pFormatCtx)<0)
        return -1; // Couldn't find stream information

        // Dump information about file onto standard error
        dump_format(pFormatCtx, 0, argv[1], 0);

        // Find the first video stream
        videoStream=-1;
        for(i=0; i<pFormatCtx->nb_streams; i++)
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
          videoStream=i;
          break;
        }
        if(videoStream==-1)
        return -1; // Didn't find a video stream

        // Get a pointer to the codec context for the video stream
        pCodecCtx=pFormatCtx->streams[videoStream]->codec;

        // Find the decoder for the video stream
        pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
        if(pCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
        }
        // Open codec
        if(avcodec_open(pCodecCtx, pCodec)<0)
        return -1; // Could not open codec

        // Allocate video frame
        pFrame=avcodec_alloc_frame();

        // Allocate an AVFrame structure
        pFrameRGB=avcodec_alloc_frame();
        if(pFrameRGB==NULL)
        return -1;

        // Determine required buffer size and allocate buffer
        numBytes=avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width,
        		      pCodecCtx->height);
        buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

        // Assign appropriate parts of buffer to image planes in pFrameRGB
        // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
        // of AVPicture
        avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24,
        	 pCodecCtx->width, pCodecCtx->height);

        // Read frames and save first five frames to disk
        i=0;
        while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {
          // Decode video frame
          avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
          
          // Did we get a video frame?
          if(frameFinished) {
        // Convert the image from its native format to RGB
        /*
        img_convert((AVPicture *)pFrameRGB, PIX_FMT_RGB24, 
                        (AVPicture*)pFrame, pCodecCtx->pix_fmt, pCodecCtx->width, 
                        pCodecCtx->height);
        				*/

        // Save the frame to disk
        if(++i<=5)
          SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, 
        	    i);
          }
        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
        }

        // Free the RGB image
        av_free(buffer);
        av_free(pFrameRGB);

        // Free the YUV frame
        av_free(pFrame);

        // Close the codec
        avcodec_close(pCodecCtx);

        // Close the video file
        av_close_input_file(pFormatCtx);
  
    }
    
	return NULL;
}
#endif

MSG_CONTEXT *g_pMsgQ = NULL;
static void get_video_frame_cb(VIDEO_FRAME * vf)
{
    struct frame *f;
    struct video_spook_input *conf;
    MSG msg;

    conf = get_conf(vf->stream);
    /*
    if(vf->stream == 1)
    {
        msg.msg = vf->stream;
        msg.param1 = vf;   
        if(post_msg(g_pMsgQ, &msg))
        {        
        }
        else
        {
            free(vf->data);
            free(vf);
        }
        
        return;
    }
    */
    //if(vf->stream == 1)
    //    printf("\r\nRcv video chunk at:%u", get_tickcount() * 10);
    if( ( f = get_next_frame( conf->ex, 0 ) ) )
	{
		f->length = vf->len;
        f->format = conf->format;
        f->width = conf->width;
        f->height = conf->height;

		f->key = vf->keyframe;
        f->d = vf->data;
        f->timestamp = vf->tick;
        free(vf);

		deliver_frame( conf->ex, f );
		//printf("\r\nDeliver packet with length %d", vf->len);
	} 
    else
	{
		//spook_log( SL_WARN, "video: dropping frame" );
        free(vf->data);
        free(vf);
	}
}

void get_video0_frame_cb(VIDEO_FRAME * vf)
{
    get_video_frame_cb(vf);
}

void get_video1_frame_cb(VIDEO_FRAME * vf)
{
    get_video_frame_cb(vf);
}

static void get_video2_frame_cb(VIDEO_FRAME * vf)
{
    get_video_frame_cb(vf);
}

static void get_video3_frame_cb(VIDEO_FRAME * vf)
{
    get_video_frame_cb(vf);
}

static GET_VIDEO_FRAME_CALLBACK g_pfGetVfCbAry[4] = 
{
    get_video0_frame_cb,
    get_video1_frame_cb,
    get_video2_frame_cb,
    get_video3_frame_cb
};

extern unsigned int g_ulChkTv;
static void get_back_frame( struct frame *f, void *d )
{
    unsigned int tStart, tNow;
	struct frame *newf;
	struct video_spook_input *conf = (struct video_spook_input *)d;

    newf = new_frame();
    if(newf)
    {
        tStart = get_tickcount();
        exchange_frame( conf->ex, newf );
        tNow = get_tickcount();
        if((tNow - tStart) > g_ulChkTv)
        {
            //printf("\r\nexchange_frame Error:%d!!!!\r\n", (tNow - tStart));            
        }
        tStart = tNow;
	    deliver_frame_to_stream( f, conf->output );
        tNow = get_tickcount();
        if((tNow - tStart) > g_ulChkTv)
        {
            //printf("\r\ndeliver_frame_to_stream Error:%d!!!!\r\n", (tNow - tStart));            
        }
    }
    else
    {
		spook_log( SL_INFO, "video get_back_frame failed");
        unref_frame(f);
    }
}

#if MACHINE_PC > 0
static void ssrc_cb(RtpSession *session)
{
	printf("hey, the ssrc has changed !\n");
}
#endif

static int video_setup( struct video_spook_input *conf )
{
#if MACHINE_PC > 0
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
#endif

    create_msg_queue(&conf->msgQ, conf->fps);
    if(conf->nStream == 1)
    {
        g_pMsgQ = &conf->msgQ;
        if( fcntl( g_pMsgQ->s_r, F_SETFL, O_NONBLOCK ) < 0 )
		spook_log( SL_INFO, "error setting O_NONBLOCK on msgQ socket: %s",
				strerror( errno ) );
    }

	return 0;
}


static void get_framerate( struct stream *s, int *fincr, int *fbase )
{
	struct video_spook_input *conf = (struct video_spook_input *)s->private;

	*fincr = conf->fincr;
	*fbase = conf->fbase;
}

static void video_get_property(struct stream *s, int argname, void *arg, int *arglen)
{
    struct video_spook_input *conf = (struct video_spook_input *)s->private;

    if(!arg || !arglen || (*arglen != sizeof(int)))
    {
        if(arglen)
            *arglen = sizeof(int);
        return -1;
    }

    switch(argname)
    {
        case VIDEO_HEIGHT:
            *(int*)arg = conf->height;
            break;

        case VIDEO_WIDTH:
            *(int*)arg = conf->width;
            break;

        case VIDEO_FINCR:
            *(int*)arg = conf->fincr;
            break;

        case VIDEO_FBASE:
            *(int*)arg = conf->fbase;
            break;

        default:
            return -1;
    }

    return 0;
}

static void set_running( struct stream *s, int running )
{
	struct video_spook_input *conf = (struct video_spook_input *)s->private;

	conf->running = running;
    if(running)
    {
        spook_log( SL_INFO, "Begin video capture, channel:%d", conf->nStream);
        c_request_video(g_pfGetVfCbAry[conf->nStream], conf->nStream);
    }
    else
    {
        spook_log( SL_INFO, "End video capture, channel:%d", conf->nStream);
        c_end_video(g_pfGetVfCbAry[conf->nStream], conf->nStream);
    }
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
    int i;
	struct video_spook_input *conf;

	conf = (struct video_spook_input *)malloc( sizeof( struct video_spook_input ) );
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
            //printf("\r\nNew Video entry, channel %d", conf->nStream);
            return conf;
        }
    }

    spook_log( SL_ERR, "video: too many channels" );
	return NULL;
}

static int end_block( void *d )
{
	struct video_spook_input *conf = (struct video_spook_input *)d;
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

    conf->width = c_get_video_width(conf->nStream);
    conf->height = c_get_video_height(conf->nStream);
	if( video_setup( conf ) < 0 ) return -1;
	conf->ex = new_exchanger( conf->fps, get_back_frame, conf );
	for( i = 0; i < (conf->fps); ++i ) 
        exchange_frame( conf->ex, new_frame() );

#if MACHINE_PC > 0
	pthread_create( &conf->thread, NULL, capture_loop, conf );
#endif

	return 0;
}

static int set_device( int num_tokens, struct token *tokens, void *d )
{
	struct video_spook_input *conf = (struct video_spook_input *)d;

	strcpy( conf->device, tokens[1].v.str );
	return 0;
}

static int set_output( int num_tokens, struct token *tokens, void *d )
{
    int nCodec;
	struct video_spook_input *conf = (struct video_spook_input *)d;

    nCodec = c_get_video_codec(conf->nStream);
    switch(nCodec)
    {
        case MP4S:
            conf->format = FORMAT_MPEG4;
            break;

        case MJPG:
            conf->format = FORMAT_JPEG;
            break;

        case H264:
            conf->format = FORMAT_H264;
            break;

        default:
            spook_log( SL_ERR,
			"video: video format %d is not support", nCodec);
            return;
    }
    
	conf->output = new_stream( tokens[1].v.str, conf->format, conf );
	if( ! conf->output )
	{
		spook_log( SL_ERR, "rtpi: unable to create stream \"%s\"",
				tokens[1].v.str );
		return -1;
	}
    spook_log( SL_INFO, "New Video Stream %s, channel %d", tokens[1].v.str, conf->nStream);
	conf->output->get_framerate = get_framerate;
	conf->output->set_running = set_running;
    conf->output->get_property = video_get_property;
    
	return 0;
}

static int set_format( int num_tokens, struct token *tokens, void *d )
{
	struct video_spook_input *conf = (struct video_spook_input *)d;

    return 0;
    
	if( conf->output )
	{
		spook_log( SL_ERR, "rtpi: output format must be specified "
				"before output stream name" );
		return -1;
	}
    
    if( ! strcasecmp( tokens[1].v.str, "mpeg4" ) )
		conf->format = FORMAT_MPEG4;
	else if( ! strcasecmp( tokens[1].v.str, "mjpeg" ) )
		conf->format = FORMAT_JPEG;
	else
	{
		spook_log( SL_ERR, "video: format \"%s\" is unsupported; try "
				"MJPEG, or MPEG4", tokens[1].v.str );
		return -1;
	}

	return 0;
}

static int set_framerate_num( int num_tokens, struct token *tokens, void *d )
{
	struct video_spook_input *conf = (struct video_spook_input *)d;

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
	struct video_spook_input *conf = (struct video_spook_input *)d;

	conf->width = tokens[1].v.num;
	conf->height = tokens[2].v.num;

	return 0;
}

static int set_port( int num_tokens, struct token *tokens, void *d )
{
	struct video_spook_input *conf = (struct video_spook_input *)d;

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

void video_init(void)
{
    int i;
#if MACHINE_PC > 0
    ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(ORTP_ERROR);
#endif

    for(i = 0; i < sizeof(conf_array)/sizeof(struct video_spook_input*); i++)
    {
        conf_array[i] = NULL;
    }

	register_config_context( "input", "video", start_block, end_block,
					config_statements );
}

int video_deinit()
{
    int i;
    
    for(i = 0; i < sizeof(conf_array)/sizeof(struct video_spook_input*); i++)
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





