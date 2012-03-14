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

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <outputs.h>
#include <rtp.h>
#include <rtp_media.h>
#include <conf_parse.h>

struct input_rtsp_source;

struct input_rtsp_session {
	struct input_rtsp_session *next;
	struct input_rtsp_session *prev;
	struct session *sess;
	struct input_rtsp_source *source;
	int playing;
};

struct input_rtsp_track {
	int index;
	struct input_rtsp_source *source;
	struct stream *stream;
	int ready;
    struct frame_exchanger* ex;
	struct rtp_media *rtp;
};

struct input_rtsp_source {
	struct input_rtsp_session *sess_list;
	struct input_rtsp_track track[MAX_TRACKS];
};

static void input_rtsp_recvframe(struct session *s, struct rtp_endpoint *ep, struct frame *f);

static int input_rtsp_get_sdp( struct session *s, char *dest, int *len,
				char *path )
{
	struct input_rtsp_session *ls = (struct input_rtsp_session *)s->private;
	int i = 0, t;
	char *addr = "IP4 0.0.0.0";

	if( ! ls->source->track[0].rtp || ! ls->source->track[0].ready )
		return 0;

	if( s->ep[0] && s->ep[0]->trans_type == RTP_TRANS_UDP )
		addr = s->ep[0]->trans.udp.sdp_addr;

	i = snprintf( dest, *len,
		"v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\ns=Test\r\na=type:broadcast\r\nt=0 0\r\nc=IN %s\r\n", addr );

	for( t = 0; t < MAX_TRACKS && ls->source->track[t].rtp; ++t )
	{
		int port;

		if( ! ls->source->track[t].ready ) return 0;

		if( s->ep[t] && s->ep[t]->trans_type == RTP_TRANS_UDP )
			port = s->ep[t]->trans.udp.sdp_port;
		else
			port = 0;

		i += ls->source->track[t].rtp->get_sdp( dest + i, *len - i,
				96 + t, port,
				ls->source->track[t].rtp->private );
		if( port == 0 ) // XXX What's a better way to do this?
			i += sprintf( dest + i, "a=control:track%d\r\n", t );
	}

	*len = i;
	return t;
}

static int input_rtsp_set_sdp( struct session *s, struct pmsg *req,	char *path )
{
    return 1;
}

static int input_rtsp_setup( struct session *s, int t )
{
	struct input_rtsp_session *ls = (struct input_rtsp_session *)s->private;
	int payload = 96 + t;

	if( ! ls->source->track[t].rtp ) 
        return -1;

    if(!s->bRecord)
    {
        return -1;
    }

	if( ls->source->track[t].rtp->get_payload )
		payload = ls->source->track[t].rtp->get_payload( payload,
					ls->source->track[t].rtp->private );
	s->ep[t] = new_rtp_endpoint( payload, 1, 8 );
	s->ep[t]->session = s;
	
	return 0;
}

static void input_rtsp_play( struct session *s, double *start )
{
	struct input_rtsp_session *ls = (struct input_rtsp_session *)s->private;
	int t;

	if( start ) *start = -1;
	ls->playing = 1;
	//for( t = 0; t < MAX_TRACKS && ls->source->track[t].rtp; ++t )
	//	if( s->ep[t] ) set_waiting( ls->source->track[t].stream, 1 );
}

static void input_rtsp_teardown( struct session *s, struct rtp_endpoint *ep )
{
	struct input_rtsp_session *ls = (struct input_rtsp_session *)s->private;
	int i, remaining = 0;

	for( i = 0; i < MAX_TRACKS && ls->source->track[i].rtp; ++i )
	{
		if( ! s->ep[i] ) continue;
		if( ! ep || s->ep[i] == ep )
		{
			del_rtp_endpoint( s->ep[i] );
			s->ep[i] = NULL;
			//track_check_running( ls->source, i );
		} else ++remaining;
	}

	if( remaining == 0 )
	{
		if( ls->next ) ls->next->prev = ls->prev;
		if( ls->prev ) ls->prev->next = ls->next;
		else ls->source->sess_list = ls->next;
		free( ls );
		del_session( s );
	}
}

static struct session *input_rtsp_open( char *path, void *d )
{
	struct input_rtsp_source *source = (struct input_rtsp_source *)d;
	struct input_rtsp_session *ls;

    if(source->sess_list)
    {
        // ok, just support one session 
        return NULL;
    }

	ls = (struct input_rtsp_session *)malloc( sizeof( struct input_rtsp_session ) );
	ls->next = source->sess_list;
	if( ls->next ) ls->next->prev = ls;
	source->sess_list = ls;
	ls->prev = NULL;
	ls->sess = new_session();
	ls->source = source;
	ls->playing = 0;
	ls->sess->get_sdp = input_rtsp_get_sdp;
    ls->sess->set_sdp = input_rtsp_set_sdp;
	ls->sess->setup = input_rtsp_setup;
	ls->sess->play = input_rtsp_play;
    ls->sess->record_data = input_rtsp_recvframe;
	ls->sess->teardown = input_rtsp_teardown;
    ls->sess->record_data = input_rtsp_recvframe;
	ls->sess->private = ls;

	return ls->sess;
}

static void input_rtsp_recvframe(struct session *s, struct rtp_endpoint *ep, struct frame *f)
{
    int i;
    struct input_rtsp_session *ls = (struct input_rtsp_session *)s->private;
    struct input_rtsp_track *track = NULL;

    for(i = 0; i < MAX_TRACKS; i++)
    {
        if(ls->sess->ep[i] == ep)
        {
            track = &ls->source->track[i];
            break;
        }
    }    

    if(!track || !track->stream || !track->ready)
    {
        unref_frame( f );
		return;
    }

    deliver_frame(track->ex, f);
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct input_rtsp_source *source;
	int i;

	source = (struct input_rtsp_source *)malloc( sizeof( struct input_rtsp_source ) );
	source->sess_list = NULL;
	for( i = 0; i < MAX_TRACKS; ++i )
	{
		source->track[i].index = i;
		source->track[i].source = source;
		source->track[i].stream = NULL;
		source->track[i].ready = 0;//@TODO:test0;
		source->track[i].rtp = NULL;
	}

	return source;
}

static int end_block( void *d )
{
    int i, j;
	struct input_rtsp_source *source = (struct input_rtsp_source *)d;

	if( ! source->track[0].rtp )
	{
		spook_log( SL_ERR, "live: no media sources specified!" );
		return -1;
	}

    for(i = 0; i < MAX_TRACKS; i++)
    {
        if(source->track[i].rtp)
        {
            //@TODO:choose a good number for slot
            source->track[i].ex = new_exchanger(8, deliver_frame_to_stream, source->track[i].stream);
            for(j = 0; j < 8; j++)
            {
                exchange_frame(source->track[i].ex, new_frame());
            }
        }
        else
        {
            break;
        }
    }

	return 0;
}

static void input_rtsp_get_framerate( struct stream *s, int *fincr, int *fbase )
{
	struct input_rtsp_track*track = (struct input_rtsp_track *)s->private;

	*fincr = 2;
	*fbase = 0;
}

static void input_rtsp_set_running( struct stream *s, int running )
{
	struct input_rtsp_track*track = (struct input_rtsp_track *)s->private;

	track->ready = running;
}

static int set_track( int num_tokens, struct token *tokens, void *d )
{
	struct input_rtsp_source *source = (struct input_rtsp_source *)d;
	int t, formats[] = { FORMAT_MPEG4, FORMAT_MPV, FORMAT_H263, FORMAT_JPEG,
				FORMAT_PCM, FORMAT_ALAW, FORMAT_MPA };

	for( t = 0; t < MAX_TRACKS && source->track[t].rtp; ++t );

	if( t == MAX_TRACKS )
	{
		spook_log( SL_ERR, "live: exceeded maximum number of tracks" );
		return -1;
	}

    //@TODO:right now, we just support audio
 	if( ! ( source->track[t].stream = new_stream( tokens[1].v.str, FORMAT_PCM, &source->track[t])))
	{
		spook_log( SL_ERR,
				"live: unable to create to stream \"%s\"",
				tokens[1].v.str );
		return -1;
	}

    source->track[t].stream->get_framerate = input_rtsp_get_framerate;
	source->track[t].stream->set_running = input_rtsp_set_running;

	switch( source->track[t].stream->format )
	{
	case FORMAT_MPEG4:
		source->track[t].rtp = new_rtp_media_mpeg4();
		break;
	case FORMAT_MPV:
		source->track[t].rtp = new_rtp_media_mpv();
		break;
	case FORMAT_H263:
		source->track[t].rtp = new_rtp_media_h263_stream(
					source->track[t].stream );
		break;
	case FORMAT_JPEG:
		source->track[t].rtp = new_rtp_media_jpeg_stream(
					source->track[t].stream );
		break;
	case FORMAT_PCM:
	case FORMAT_ALAW:
		source->track[t].rtp = new_rtp_media_rawaudio_stream(
					source->track[t].stream );
		break;
	case FORMAT_MPA:
		source->track[t].rtp = new_rtp_media_mpa();
		break;
	}

	if( ! source->track[t].rtp ) return -1;	

	return 0;
}

static int set_path( int num_tokens, struct token *tokens, void *d )
{
	if( num_tokens == 2 )
	{
		new_rtsp_location( tokens[1].v.str, NULL, NULL, NULL,
				input_rtsp_open, d );
		return 0;
	}
	if( num_tokens == 5 )
	{
		new_rtsp_location( tokens[1].v.str, tokens[2].v.str,
				tokens[3].v.str, tokens[4].v.str,
				input_rtsp_open, d );
		return 0;
	}
	spook_log( SL_ERR, "rtsp-handler: syntax: Path <path> [<realm> <username> <password>]" );
	return -1;
}

#if 0
static int set_sip_line( int num_tokens, struct token *tokens, void *d )
{
	new_sip_line( tokens[1].v.str, input_rtsp_open, d );
	return 0;
}
#endif

static struct statement config_statements[] = {
	/* directive name, process function, min args, max args, arg types */
	{ "track", set_track, 1, 1, { TOKEN_STR } },
	{ "path", set_path, 1, 4, { TOKEN_STR, TOKEN_STR, TOKEN_STR, TOKEN_STR } },
	/* { "sipline", set_sip_line, 1, 1, { TOKEN_STR } }, */

	/* empty terminator -- do not remove */
	{ NULL, NULL, 0, 0, {} }
};

int input_rtsp_init(void)
{
	register_config_context( "input", "rtsp", start_block, end_block,
					config_statements );
	return 0;
}

