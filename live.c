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
#include "inputs.h"
#include "include/libcomponent.h"
#include "include/list.h"
#include "include/av_type.h"
#include "include/c_audio.h"
#include "include/c_user.h"
#include "include/c_video.h"
#include "include/utility.h"

#define LIVE_MAX_TRACKS  5
#define LIVE_MAX_ENTRIES 4
struct live_entry;

//@TODO: free resource when exit
//@TODO: rtp timestamp
struct live_session {
	struct live_session *next;
	struct live_session *prev;
	struct session *rtspsess;
	struct live_entry *liventry;
	int playing;
};

struct live_track {
	struct live_entry *entrylist[LIVE_MAX_ENTRIES];
	struct stream_destination *srcstream;// source stream of track
	int ready;
    struct in_addr addr;   // local endian
    unsigned short port;   // local endian
    int tick2tsratio;
	struct rtp_media *rtp;// RTP entry serv for many destination endpoints
	char szName[32];
};

struct live_entry {
    char szRealm[128];
	struct live_session *sess_list;
	struct live_track *tracklist[MAX_TRACKS];
};

struct live_track g_tracks[LIVE_MAX_TRACKS];

int g_bMulticastMode = 0;
bool g_bStopSend = false;

static int live_get_sdp( struct session *s, char *dest, int *len,
				char *path )
{
	struct live_session *ls = (struct live_session *)s->private;
	int i = 0, t;
    int height = 0;
    int width = 0;
    int arglen = sizeof(int);
	char *addr = "0.0.0.0";
    struct stream *st;

	if( ! ls->liventry->tracklist[0]->rtp || ! ls->liventry->tracklist[0]->ready )
		return 0;

	if( s->ep[0] && s->ep[0]->trans_type != RTP_TRANS_INTER)
		addr = s->ep[0]->trans.udp.sdp_addr;
    else if(g_bMulticastMode)
        addr = "239.5.6.7";

	i = snprintf( dest, *len,
		"v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\ns=Test\r\na=type:broadcast\r\nt=0 0\r\nc=IN IP4 %s/1\r\n", addr );

	for( t = 0; t < MAX_TRACKS && ls->liventry->tracklist[t] && ls->liventry->tracklist[t]->rtp; ++t )
	{
		int port;

		if( ! ls->liventry->tracklist[t]->ready ) return 0;

		if( s->ep[t] && s->ep[t]->trans_type != RTP_TRANS_INTER )
			port = s->ep[t]->trans.udp.sdp_port;
		else
			port = (ls->liventry->tracklist[t]->port);

		i += ls->liventry->tracklist[t]->rtp->get_sdp( dest + i, *len - i,
				96 + t, port,
				ls->liventry->tracklist[t]->rtp->private );
        if(g_bMulticastMode)
            i += sprintf( dest + i, "c=IN IP4 %s/1\r\n", addr );
		//if( port == 0 ) // XXX What's a better way to do this?
			i += sprintf( dest + i, "a=control:track%d\r\n", t );
        height = width = 0;
        st = ls->liventry->tracklist[t]->srcstream->stream;
        if( st->get_property )
        {
            st->get_property(st, VIDEO_HEIGHT, &height, &arglen);
            st->get_property(st, VIDEO_WIDTH, &width, &arglen);
        }

        if(height && width)
        {
            i += sprintf( dest + i, "a=x-dimensions:%d,%d\r\n", width, height );        
        }
	}

	*len = i;
	return t;
}

// handle of "SETUP" request, build relation between ep and rtsp session
static int live_setup( struct session *s, int t)
{
	struct live_session *ls = (struct live_session *)s->private;
	int payload = 96 + t;

	if( !ls->liventry->tracklist[t] || ! ls->liventry->tracklist[t]->rtp ) return -1;

	if( ls->liventry->tracklist[t]->rtp->get_payload )
		payload = ls->liventry->tracklist[t]->rtp->get_payload( payload,
					ls->liventry->tracklist[t]->rtp->private );
    
    if(g_bMulticastMode)
    {
        struct rtp_endpoint *ep_mc = ls->liventry->tracklist[t]->rtp->ep_mc;
        if(!ep_mc)
        {
            ep_mc = new_rtp_endpoint( payload, ls->liventry->tracklist[t]->tick2tsratio, 0, 0 );
            if(!ep_mc)
            {
                return 1;
            }

            ls->liventry->tracklist[t]->rtp->ep_mc = ep_mc;
            connect_mudp_endpoint(ep_mc, 
                ls->liventry->tracklist[t]->addr, 
                ls->liventry->tracklist[t]->port, 
                ls->liventry->tracklist[t]->port + 1);
            ep_mc->session = NULL;            
        }

        s->ep[t] = ep_mc;
        s->ep[t]->session = NULL;
    }
	else
	{
        s->ep[t] = new_rtp_endpoint( payload, ls->liventry->tracklist[t]->tick2tsratio, 0, 0 );
        if(s->ep[t] == NULL)
        {
            spook_log( SL_ERR, "live: new_rtp_endpoint failed!" );
        }
    	s->ep[t]->session = s;
	}
    
	return 0;
}

static void live_play( struct session *s, double *start )
{
	struct live_session *ls = (struct live_session *)s->private;
	int t;

	if( start ) *start = -1;
	ls->playing = 1;
	for( t = 0; t < MAX_TRACKS && 
                ls->liventry->tracklist[t] && 
                ls->liventry->tracklist[t]->rtp; ++t )
	{   
		if( s->ep[t] ) 
		{
            set_waiting( ls->liventry->tracklist[t]->srcstream, 1 );
            if(s->ep[t]->packet_count != 0)
            {
                *start = 0;
            }
		}
	}
}

static void track_check_running( struct live_entry *entry, int t )
{
	struct live_session *ls;

    // check if other RTP ep exist in the same rtsp session
	for( ls = entry->sess_list; ls; ls = ls->next )
		if( ls->playing && ls->rtspsess->ep[t] ) return;

    // we are the last ep, so temp disable track 
	set_waiting( entry->tracklist[t]->srcstream, 0 );
}

static void track_check_running1( struct live_track *track, struct rtp_media *rtp)
{
    int i, j;
	struct live_session *ls;

    for(i = 0; i < LIVE_MAX_ENTRIES && track->entrylist[i]; i++)
    {
        // check other live entries
        for(j = 0; j < MAX_TRACKS; j++)
        {
            // we are on which position?
            if(track->entrylist[i]->tracklist[j] &&
                (track->entrylist[i]->tracklist[j] == track))
            {
                // check 
            	for( ls = track->entrylist[i]->sess_list; ls; ls = ls->next )
            	{
            		if( ls->playing && ls->rtspsess->ep[j] )
                        return;    
            	}
            }
        }
    }

    // we are the last ep, so temp disable track 
	set_waiting( track->srcstream, 0 );
    if(g_bMulticastMode)
    {
        del_rtp_endpoint(rtp->ep_mc);
        rtp->ep_mc = NULL;
    }
}

static void live_teardown( struct session *s, struct rtp_endpoint *ep )
{
	struct live_session *ls = (struct live_session *)s->private;
	int i, remaining = 0;

    // if ep=0, all the tracks of session need teardown
	for( i = 0; 
    i < MAX_TRACKS && 
        ls->liventry->tracklist[i] && 
        ls->liventry->tracklist[i]->rtp; 
    ++i )
	{
		if( ! s->ep[i] ) continue;
		if( ! ep || s->ep[i] == ep )
		{
            if(!g_bMulticastMode)
    			del_rtp_endpoint( s->ep[i] );
			s->ep[i] = NULL;
			//track_check_running( ls->liventry, i );
			track_check_running1(ls->liventry->tracklist[i], ls->liventry->tracklist[i]->rtp);
		} else ++remaining;
	}

	if( remaining == 0 )
	{
		if( ls->next ) ls->next->prev = ls->prev;
		if( ls->prev ) ls->prev->next = ls->next;
		else ls->liventry->sess_list = ls->next;
		free( ls );
		del_session( s );
	}
}

/* a new RTSP client connect to me, open a session for it */
static struct session *live_open( char *path, void *d )
{
	struct live_entry *entry = (struct live_entry *)d;
	struct live_session *ls;

	ls = (struct live_session *)malloc( sizeof( struct live_session ) );
    // put the node at the first position of the list
	ls->next = entry->sess_list;
	if( ls->next ) 
        ls->next->prev = ls;
	entry->sess_list = ls;
    
	ls->prev = NULL;
	ls->rtspsess = new_session();
	ls->liventry = entry;
	ls->playing = 0;
	ls->rtspsess->get_sdp = live_get_sdp;
	ls->rtspsess->setup = live_setup;
	ls->rtspsess->play = live_play;
	ls->rtspsess->teardown = live_teardown;
	ls->rtspsess->private = ls;

    spook_log( SL_INFO, "Open live, path:%s, entry:%s", path, entry->szRealm);

	return ls->rtspsess;
}

extern unsigned int g_ulChkTv;
static void next_live_frame( struct frame *f, void *d )
{
    int i, j, ret;
	struct live_track *track = (struct live_track *)d;
	struct live_session *ls, *next;
    unsigned int tStart, tNow;

    tStart = get_tickcount();
	if( ! track->rtp->frame( f, track->rtp->private ) )
	{
		unref_frame( f );
		return;
	}

    tNow = get_tickcount();
        if((tNow - tStart) > g_ulChkTv)
        {
            //printf("\r\nrtp->frame Error:%d!!!!\r\n", (tNow - tStart));            
        }
        tStart = tNow;

	if( ! track->ready )
	{
		set_waiting( track->srcstream, 0 );
		track->ready = 1;
	}

    for(i = 0; i < LIVE_MAX_ENTRIES; i++)
    {
        if(track->entrylist[i] == NULL)
        {
            break;
        }

        if(track->rtp->ep_mc)
        {
            tStart = get_tickcount();
            track->rtp->send( track->rtp->ep_mc,
    						track->rtp->private );
            tNow = get_tickcount();
            if((tNow - tStart) > g_ulChkTv)
            {
                //printf("\r\nrtp->send Error:%d!!!!\r\n", (tNow - tStart));            
            }
            break;
        }
        
    	for( ls = track->entrylist[i]->sess_list; ls; ls = next )
    	{
    		next = ls->next;
            for(j = 0; j < MAX_TRACKS; j++)
            {
                if(track->entrylist[i]->tracklist[j] == track)
                {
                    struct rtp_endpoint *ep = ls->rtspsess->ep[j];                    
                    
                    if( ls->playing && ep )
                    {
                        if(ep->bLostKeyFrame && !f->key)
                        {
                            continue;
                        }

                        if(g_bStopSend)
                        {
                            ret = -1;
                        }
                        else
                        {
                            ret = track->rtp->send( ep, track->rtp->private );
                        }
                        
            			if(ret != 0)
            			{
                            if(f->key)
                            {
                                ep->bLostKeyFrame = true;
                            }
            			}
                        else
                        {
                            if(f->key)
                            {
                                ep->bLostKeyFrame = false;
                            }
                        }
                    }
                    
                    break;
                }
            }
    	}
    }

	unref_frame( f );
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct live_entry *source;
	int i;

	source = (struct live_entry *)malloc( sizeof( struct live_entry ) );
	source->sess_list = NULL;
	for( i = 0; i < MAX_TRACKS; ++i )
	{
		source->tracklist[i] = NULL;
	}

	return source;
}

static int end_block( void *d )
{
    int i;
	struct live_entry *source = (struct live_entry *)d;

	if( ! source->tracklist[0]->rtp )
	{
		spook_log( SL_ERR, "live: no media sources specified!" );
		return -1;
	}

    if(source->tracklist[1])
    {
        spook_log( SL_INFO, 
                    "New Live Entry %s----track %d:%s, track %d:%s", 
                    source->szRealm,
                    0, source->tracklist[0]->szName,
                    1, source->tracklist[1]->szName );
    }
    else
    {
        spook_log( SL_INFO, 
                    "New Live Entry %s----track %d:%s", 
                    source->szRealm,
                    0, source->tracklist[0]->szName);
    }

	return 0;
}

static int set_track( int num_tokens, struct token *tokens, void *d )
{
	struct live_entry *entry = (struct live_entry *)d;
	int i, t, formats[] = { FORMAT_MPEG4, /*FORMAT_MPV, FORMAT_H263,*/ FORMAT_JPEG,
				FORMAT_PCM, /*FORMAT_ALAW, FORMAT_MPA,*/ FORMAT_H264, FORMAT_ADPCM };

	for( t = 0; t < MAX_TRACKS && entry->tracklist[t]; ++t );

	if( t == MAX_TRACKS )
	{
		spook_log( SL_ERR, "live: exceeded maximum number of tracks" );
		return -1;
	}

    for(i = 0; i < LIVE_MAX_TRACKS; i++)
    {
        if(strcmp(g_tracks[i].szName, tokens[1].v.str) == 0)
        {
            // this track has setup, join it
            entry->tracklist[t] = &g_tracks[i];
            for(i = 0; i < LIVE_MAX_ENTRIES; i++)
            {
                if(entry->tracklist[t]->entrylist[i] == NULL)
                {
                   entry->tracklist[t]->entrylist[i] = entry; 
                   break;
                }
            }

            if(i == LIVE_MAX_ENTRIES)
            {
                entry->tracklist[t] = NULL;
                spook_log( SL_ERR, "live: exceeded maximum number of entries" );
        		return -1;
            }
            
            return 0;
        }
    }

    for(i = 0; i < LIVE_MAX_TRACKS; i++)
    {
        if(!g_tracks[i].rtp)
        {
            break;
        }
    }

    if(i == LIVE_MAX_TRACKS)
    {
        entry->tracklist[t] = NULL;
        spook_log( SL_ERR, "live: exceeded maximum number of tracks" );
		return -1;
    }
    
	if( ! ( g_tracks[i].srcstream = connect_to_stream( tokens[1].v.str,
			next_live_frame, &g_tracks[i], formats, sizeof(formats)/sizeof(int) ) ) )
	{
		spook_log( SL_ERR,
				"live: unable to connect to stream \"%s\"",
				tokens[1].v.str );
		return -1;
	}

	switch( g_tracks[i].srcstream->stream->format )
	{
	case FORMAT_MPEG4:
		g_tracks[i].rtp = new_rtp_media_mpeg4();
        g_tracks[i].tick2tsratio = 900;
		break;
        /*
	case FORMAT_MPV:
		g_tracks[i].rtp = new_rtp_media_mpv();
		break;
	case FORMAT_H263:
		g_tracks[i].rtp = new_rtp_media_h263_stream(
					g_tracks[i].srcstream->stream );
					*/
    case FORMAT_H264:
		g_tracks[i].rtp = new_rtp_media_h264_stream(
					g_tracks[i].srcstream->stream );
        g_tracks[i].tick2tsratio = 900;
		break;
	case FORMAT_JPEG:
		g_tracks[i].rtp = new_rtp_media_jpeg_stream(
					g_tracks[i].srcstream->stream );
        g_tracks[i].tick2tsratio = 900;
		break;
	case FORMAT_PCM:
	//case FORMAT_ALAW:
    case FORMAT_ADPCM:
		g_tracks[i].rtp = new_rtp_media_rawaudio_stream(
					g_tracks[i].srcstream->stream );
        g_tracks[i].tick2tsratio = 80;
		break;
        /*
	case FORMAT_MPA:
		g_tracks[i].rtp = new_rtp_media_mpa();
		break;
		*/
	}

	if( ! g_tracks[i].rtp ) 
    {
        spook_log( SL_ERR, "live: create RTP stream error:%s", tokens[1].v.str );
        return -1;
	}

	set_waiting( g_tracks[i].srcstream, 1 );//trigger track to ready
    strncpy(g_tracks[i].szName, tokens[1].v.str, sizeof(g_tracks[i].szName));
    if(g_bMulticastMode)
    {
        g_tracks[i].addr.s_addr = htonl(inet_addr("239.5.6.7"));
        g_tracks[i].port = (54320 + (i << 1));
    }

    entry->tracklist[t] = &g_tracks[i];
    g_tracks[i].entrylist[0] = entry;    
    
	return 0;
}

static int set_path( int num_tokens, struct token *tokens, void *d )
{
    struct live_entry *entry = (struct live_entry *)d;
    strncpy(entry->szRealm, tokens[1].v.str, sizeof(entry->szRealm));
    
	if( num_tokens == 2 )
	{
		new_rtsp_location( tokens[1].v.str, NULL, NULL, NULL,
				live_open, d );
		return 0;
	}
	if( num_tokens == 5 )
	{
		new_rtsp_location( tokens[1].v.str, tokens[2].v.str,
				tokens[3].v.str, tokens[4].v.str,
				live_open, d );
		return 0;
	}
	spook_log( SL_ERR, "rtsp-handler: syntax: Path <path> [<realm> <username> <password>]" );
	return -1;
}

#if 0
static int set_sip_line( int num_tokens, struct token *tokens, void *d )
{
	new_sip_line( tokens[1].v.str, live_open, d );
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

int live_init(void)
{
    memset(g_tracks, 0, sizeof(g_tracks));
	register_config_context( "rtsp-handler", "live", start_block, end_block,
					config_statements );    
    
	return 0;
}

//@TODO: 
int live_deinit()
{
    
}

