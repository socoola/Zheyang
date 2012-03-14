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

struct rtp_h264 {
	unsigned char *d;
	int len;
	int init_done;
	int ts_incr;
	unsigned int timestamp;
};

static int h264_process_frame( struct frame *f, void *d )
{
	struct rtp_h264 *out = (struct rtp_h264 *)d;

	/* Discard the first two bytes, which are both 0x00 */
    int i;

    if((f->d[0] != 0x00) || (f->d[1] != 0x00))
    {
        //error packet
        printf("\r\nH264 start code error!");
        return 0;
    }
    
    for(i = 2; i < 4; i++)
    {
        if(f->d[i] == 0x01)
        {
            out->d = f->d + i + 1;
            out->len = f->length - i - 1;
            if(f->timestamp)
            {
                
                out->timestamp = f->timestamp;
            }
            else
                out->timestamp += out->ts_incr;
        	return out->init_done;
        }
        else if(f->d[i] != 0x00)
        {
            break;
        }
    }

    printf("\r\nH264 start code error!");
    return 0;
}

static int h264_get_sdp( char *dest, int len, int payload, int port, void *d )
{
    struct rtp_h264 *out = (struct rtp_h264 *)d;    
    
	return snprintf( dest, len, "m=video %d RTP/AVP %d\r\na=rtpmap:%d H264/90000\r\n"
                                "a=fmtp:%d packetization-mode=1\r\n",                                 
                                port, payload, payload, payload);
}

static int h264_send( struct rtp_endpoint *ep, void *d )
{
	struct rtp_h264 *out = (struct rtp_h264 *)d;
	int i, plen;
	struct iovec v[3];
	unsigned char vhdr[2]; /* Set P bit */

	v[1].iov_base = vhdr;
	v[1].iov_len = 2;

    if(out->len > ep->max_data_size)
    {
        vhdr[0] = (out->d[0] & 0xE0) | 28;
        vhdr[1] = (out->d[0] & 0x1F) | 0x80;
    }
    else
    {
        v[1].iov_base = out->d;
		v[1].iov_len = out->len;
        if( send_rtp_packet( ep, v, 2, out->timestamp, 1 ) < 0 )
			return -1;
        return 0;
    }

	for( i = 1; i < out->len; i += plen )
	{
		plen = out->len - i;
		if( plen > (ep->max_data_size - 2) )
            plen = ep->max_data_size - 2;
        else
            vhdr[1] |= 0x40;
		v[2].iov_base = out->d + i;
		v[2].iov_len = plen;
		if( send_rtp_packet( ep, v, 3, out->timestamp,
						plen + i == out->len ) < 0 )
			return -1;
		vhdr[1] &= ~0x80; // FU header (no S bit)
	}
	return 0;
}

struct rtp_media *new_rtp_media_h264_stream( struct stream *stream )
{
	struct rtp_h264 *out;
	int fincr, fbase;

	stream->get_framerate( stream, &fincr, &fbase );
	out = (struct rtp_h264 *)malloc( sizeof( struct rtp_h264 ) );
	out->init_done = 1;
	out->timestamp = 0;
	out->ts_incr = 90000 * fincr / fbase;

	return new_rtp_media( h264_get_sdp, NULL,
					h264_process_frame, h264_send, out );
}

