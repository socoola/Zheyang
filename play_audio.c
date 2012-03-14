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
#include "include/libcomponent.h"
#include "include/list.h"
#include "include/av_type.h"
#include "include/c_audio.h"
#include "include/c_user.h"
#include "include/c_video.h"
#include "include/utility.h"

struct play_audio_source;

struct play_audio_track {
	int index;
	struct play_audio_source *source;
	struct stream_destination *stream;
	int ready;
	struct rtp_media *rtp;
};

struct play_audio_source {
	struct play_audio_track track[MAX_TRACKS];
};

static void next_play_audio_frame( struct frame *f, void *d )
{
	struct play_audio_track *track = (struct play_audio_track *)d;

	AUDIO_CHUNK *pChunk = malloc(sizeof(AUDIO_CHUNK));
    if(pChunk)
    {
        switch(f->format)
        {
            case FORMAT_PCM:
                pChunk->codec = PCM;
                break;

            case FORMAT_ADPCM:
                pChunk->codec = ADPCM;
                break;

            default:
                break;
                
        }
        
        pChunk->seq = f->seq;
        pChunk->tick = f->timestamp>>3;
        pChunk->t = 0;
        pChunk->sample = ntohs(*(unsigned short*)f->d);
        pChunk->index = ntohs(*(unsigned short*)(f->d + 2));
        pChunk->len = f->length - 4;
        pChunk->data = malloc(pChunk->len);
        if(pChunk->data)
        {
            memcpy(pChunk->data, f->d + 4, pChunk->len);
            INIT_LIST_HEAD(&pChunk->list);
            c_speak(pChunk);
        }
        else
        {
            free(pChunk);
        }

        
    }

	unref_frame( f );
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct play_audio_source *source;
	int i;

	source = (struct play_audio_source *)malloc( sizeof( struct play_audio_source ) );
	source->sess_list = NULL;
	for( i = 0; i < MAX_TRACKS; ++i )
	{
		source->track[i].index = i;
		source->track[i].source = source;
		source->track[i].stream = NULL;
		source->track[i].ready = 0;
		source->track[i].rtp = NULL;
	}

	return source;
}

static int end_block( void *d )
{
	struct play_audio_source *source = (struct play_audio_source *)d;

	if( ! source->track[0].stream )
	{
		spook_log( SL_ERR, "live: no media sources specified!" );
		return -1;
	}

	return 0;
}

static int set_track( int num_tokens, struct token *tokens, void *d )
{
	struct play_audio_source *source = (struct play_audio_source *)d;
	int t, formats[] = { FORMAT_PCM, FORMAT_ADPCM };

	for( t = 0; t < MAX_TRACKS && source->track[t].rtp; ++t );

	if( t == MAX_TRACKS )
	{
		spook_log( SL_ERR, "live: exceeded maximum number of tracks" );
		return -1;
	}

	if( ! ( source->track[t].stream = connect_to_stream( tokens[1].v.str,
			next_play_audio_frame, &source->track[t], formats, 2 ) ) )
	{
		spook_log( SL_ERR,
				"live: unable to connect to stream \"%s\"",
				tokens[1].v.str );
		return -1;
	}

	set_waiting( source->track[t].stream, 1 );

	return 0;
}

static struct statement config_statements[] = {
	/* directive name, process function, min args, max args, arg types */
	{ "track", set_track, 1, 1, { TOKEN_STR } },
	//{ "path", set_path, 1, 4, { TOKEN_STR, TOKEN_STR, TOKEN_STR, TOKEN_STR } },
	/* { "sipline", set_sip_line, 1, 1, { TOKEN_STR } }, */

	/* empty terminator -- do not remove */
	{ NULL, NULL, 0, 0, {} }
};

int play_audio_init(void)
{
	register_config_context( "play", "audio", start_block, end_block,
					config_statements );
	return 0;
}

