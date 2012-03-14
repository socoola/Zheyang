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
#include <arpa/inet.h>
#include <errno.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <rtp.h>
#include <conf_parse.h>
#include "include/libcomponent.h"
#include "include/list.h"
#include "include/av_type.h"
#include "include/c_audio.h"
#include "include/c_user.h"
#include "include/c_video.h"
#include "include/utility.h"

static int rtp_port_start = 50000, rtp_port_end = 60000;

static int rtcp_send( struct rtp_endpoint *ep );
int g_bForceRtcp = 1;
unsigned int g_ulLocalAddr = 0, g_nDroprateThreshold = 50;//network endian
static void rtcp_fire( struct event_info *ei, void *d )
{
	struct rtp_endpoint *ep = (struct rtp_endpoint *)d;

	rtcp_send( ep );
}

static void rtp_deliver_frame( struct frame *f, void *d )
{
    struct rtp_endpoint *ep = (struct rtp_endpoint *)d;

    ep->session->record_data(ep->session, ep, f);
}

struct rtp_endpoint *new_rtp_endpoint( int payload, int tick2tsratio, int bRecord, int nRecordQSize)
{
	struct rtp_endpoint *ep;

	if( ! ( ep = (struct rtp_endpoint *)
			malloc( sizeof( struct rtp_endpoint ) ) ) )
		return NULL;
	ep->payload = payload;
	ep->max_data_size = 8192; /* default maximum */
	ep->ssrc = 0;
	random_bytes( (unsigned char *)&ep->ssrc, 4 );
	ep->start_timestamp = 0;
    ep->recv_start_ts = 0;
	random_bytes( (unsigned char *)&ep->start_timestamp, 4 );
	ep->last_timestamp = ep->start_timestamp;
	ep->seqnum = 0;
	random_bytes( (unsigned char *)&ep->seqnum, 2 );
	ep->packet_count = 0;
	ep->octet_count = 0;
	ep->rtcp_send_event = add_timer_event( 1000, 0, rtcp_fire, ep );
	set_event_enabled( ep->rtcp_send_event, 0 );
	ep->force_rtcp = g_bForceRtcp;
	gettimeofday( &ep->last_rtcp_recv, NULL );
	ep->trans_type = 0;
    ep->tick2tsratio = tick2tsratio;
    ep->send_drop_rate = 0;
    ep->bLostKeyFrame = true;
    ep->bTriggerWirelessCheck = false;

    if(bRecord)
    {
        int i;
        ep->ex = new_exchanger(nRecordQSize, rtp_deliver_frame, ep);
        for(i = 0; i < nRecordQSize; i++)
        {
            exchange_frame(ep->ex, new_frame());
        }
    }
    else
    {
        ep->ex = NULL;
    }

	return ep;
}

void del_rtp_endpoint( struct rtp_endpoint *ep )
{
    if(ep->bTriggerWirelessCheck)
    {
        end_wireless_check();
    }
    
	remove_event( ep->rtcp_send_event );

	switch( ep->trans_type )
	{
	case RTP_TRANS_UDP:
    case RTP_TRANS_MC:
		remove_event( ep->trans.udp.rtp_event );
		close( ep->trans.udp.rtp_fd );
		remove_event( ep->trans.udp.rtcp_event );
		close( ep->trans.udp.rtcp_fd );
		break;
	case RTP_TRANS_INTER:
		interleave_disconnect( ep->trans.inter.conn,
						ep->trans.inter.rtp_chan );
		interleave_disconnect( ep->trans.inter.conn,
						ep->trans.inter.rtcp_chan );
		break;
	}

	free( ep );
}

void update_rtp_timestamp( struct rtp_endpoint *ep, int time_increment )
{
	ep->last_timestamp += time_increment;
	ep->last_timestamp &= 0xFFFFFFFF;
}

static void udp_rtp_read( struct event_info *ei, void *d )
{
	struct rtp_endpoint *ep = (struct rtp_endpoint *)d;
	unsigned char buf[1024];
	int ret;

	ret = read( ep->trans.udp.rtp_fd, buf, sizeof( buf ) );
	if( ret > 0 )
	{
        struct frame *f;
        
		/* some SIP phones don't send RTCP */
		gettimeofday( &ep->last_rtcp_recv, NULL );
        if(ep->recv_start_ts == 0)
        {
            ep->recv_start_ts = GET_32(buf + 4);
        }
        
        if(ep->ex)
        {
            f = get_next_frame(ep->ex, 0);
            if(f)
            {
                f->length = (ret - 12) > ep->max_data_size ? ep->max_data_size:(ret - 12);
        		f->format = 0;
        		f->width = 0;
        		f->height = 0;
                f->seq = GET_16(buf + 2);
                f->timestamp = GET_32(buf + 4) - ep->recv_start_ts;
        		f->key = (buf[1] & 0x80)?1:0;
                f->d = malloc(f->length);
                if(f->d)
                {
                    memcpy(f->d, buf, f->length);
                    deliver_frame(ep->ex, f);
                }
                else
                {
                    unref_frame(f);
                    spook_log( SL_WARN, "drop input packet because of malloc fail");
                }
            }
            else
            {
                spook_log( SL_WARN, "drop input packet because of get_next_frame fail");
            }
        }
        
		return;
	} 
    else if( ret < 0 )
    {
		spook_log( SL_VERBOSE, "error on UDP RTP socket: %s",
			strerror( errno ) );
    }
	else 
    {
        spook_log( SL_VERBOSE, "UDP RTP socket closed" );
	}

    if(ep->session)
    	ep->session->teardown( ep->session, ep );
}

static void udp_rtcp_read( struct event_info *ei, void *d )
{
	struct rtp_endpoint *ep = (struct rtp_endpoint *)d;
	unsigned char buf[1024];
	int ret;

	ret = read( ep->trans.udp.rtcp_fd, buf, sizeof( buf ) );
	if( ret > 0 )
	{
		//spook_log( SL_DEBUG, "received RTCP packet from client" );
		//@TODO: parse drop rate
		if((buf[1] == 0xc9) && (buf[2] == 0x00) && (buf[3] == 0x07))
		{
            ep->send_drop_rate = buf[12];
            if(ep->send_drop_rate >= g_nDroprateThreshold)
            {
                ep->bTriggerWirelessCheck = true;
                begin_wireless_check();
                spook_log( SL_INFO, "%08x recv rtcp loss report: %d", ep, ep->send_drop_rate );
            }
            else if(ep->send_drop_rate == 0)
            {
                ep->bTriggerWirelessCheck = false;
                end_wireless_check();
            }
            
    		gettimeofday( &ep->last_rtcp_recv, NULL );
		}
        
		return;
	} else if( ret < 0 )
		spook_log( SL_VERBOSE, "error on UDP RTCP socket: %s",
			strerror( errno ) );
	else spook_log( SL_VERBOSE, "UDP RTCP socket closed" );
    if(ep->session)
    	ep->session->teardown( ep->session, ep );
}

void interleave_recv_rtcp( struct rtp_endpoint *ep, unsigned char *d, int len )
{
	spook_log( SL_DEBUG, "received RTCP packet from client" );
	gettimeofday( &ep->last_rtcp_recv, NULL );
}

static int rtcp_send( struct rtp_endpoint *ep )
{
	struct timeval now;
	unsigned char buf[128]; // if you add content to rtcp packet, notice here!!!
	unsigned int ntp_sec, ntp_usec;
	struct iovec v[1];

	gettimeofday( &now, NULL );

	ep->force_rtcp = 0;
//	spook_log( SL_DEBUG, "sending RTCP packet" );

	/* Grrr...  QuickTime apparently doesn't send RTCP over TCP */
    //@TODO:check rtcp packet interval
	if( ep->trans_type == RTP_TRANS_UDP )
	{
		if(( now.tv_sec - ep->last_rtcp_recv.tv_sec > 1 ) ||
            ((now.tv_sec - ep->last_rtcp_recv.tv_sec == 1) &&
            (now.tv_usec - ep->last_rtcp_recv.tv_usec > 200000)))
		{
			spook_log( SL_VERBOSE, 
                "client %08x timeout (no RTCP received in %d seconds)", 
                ep, now.tv_sec - ep->last_rtcp_recv.tv_sec);
            begin_wireless_check();
            if( now.tv_sec - ep->last_rtcp_recv.tv_sec > 15 )
            {
                if(ep->session)
        			ep->session->teardown( ep->session, ep );
    			return -1;
            }
		}
	}

    return;

	ntp_sec = now.tv_sec + 0x83AA7E80;
	ntp_usec = (double)( (double)now.tv_usec * (double)0x4000000 ) / 15625.0;
	
	//spook_log( SL_DEBUG, "ssrc=%u, ntp_sec=%u, ntp_usec=%u last_timestamp=%u packet_count=%d octet_count=%d",
	//		x->ssrc, ntp_sec, ntp_usec, x->last_timestamp, x->packet_count, x->octet_count );

	buf[0] = 2 << 6; // version
	buf[1] = 200; // packet type is Sender Report
	PUT_16( buf + 2, 6 ); // length in words minus one
	PUT_32( buf + 4, ep->ssrc );
	PUT_32( buf + 8, ntp_sec );
    ntp_usec += 0.5;
	PUT_32( buf + 12, ntp_usec );
	PUT_32( buf + 16, get_tickcount() * ep->tick2tsratio + ep->start_timestamp);//ep->last_timestamp );
	PUT_32( buf + 20, ep->packet_count );
	PUT_32( buf + 24, ep->octet_count );
	buf[28] = ( 2 << 6 ) | 1; // version; source count = 1
	buf[29] = 202; // packet type is Source Description
	PUT_16( buf + 30, 5 ); // length in words minus one
	PUT_32( buf + 32, ep->ssrc );
	buf[36] = 0x01; // field type is CNAME
	buf[37] = 14; // text length
	memcpy( buf + 38, "Unnamed stream", 14 );
	switch( ep->trans_type )
	{
    case RTP_TRANS_MC:
	case RTP_TRANS_UDP:
		if( send( ep->trans.udp.rtcp_fd, buf, 52, 0 ) < 0 )
			spook_log( SL_VERBOSE, "error sending UDP RTCP frame: %s",
					strerror( errno ) );
		else return 0;
		break;
	case RTP_TRANS_INTER:
		v[0].iov_base = buf;
		v[0].iov_len = 52;
		if( interleave_send( ep->trans.inter.conn,
				ep->trans.inter.rtcp_chan, v, 1 ) < 0 )
			spook_log( SL_VERBOSE, "error sending interleaved RTCP frame" );
		else return 0;
		break;
	}
    if(ep->session)
    	ep->session->teardown( ep->session, ep );
	return -1;
}

int connect_udp_endpoint( struct rtp_endpoint *ep,
		struct in_addr dest_ip, int dest_port, int *our_port )
{
	struct sockaddr_in rtpaddr, rtcpaddr;
	int port, success = 0, i, max_tries, rtpfd = -1, rtcpfd = -1;

	rtpaddr.sin_family = rtcpaddr.sin_family = AF_INET;
	rtpaddr.sin_addr.s_addr = rtcpaddr.sin_addr.s_addr = g_ulLocalAddr;

	port = rtp_port_start + random() % ( rtp_port_end - rtp_port_start );
	if( port & 0x1 ) ++port;
	max_tries = ( rtp_port_end - rtp_port_start + 1 ) / 2;

	for( i = 0; i < max_tries; ++i )
	{
		if( port + 1 > rtp_port_end ) port = rtp_port_start;
		rtpaddr.sin_port = htons( port );
		rtcpaddr.sin_port = htons( port + 1 );
		if( rtpfd < 0 &&
			( rtpfd = socket( PF_INET, SOCK_DGRAM, 0 ) ) < 0 )
		{
			spook_log( SL_WARN, "unable to create UDP RTP socket: %s",
					strerror( errno ) );
			return -1;
		}
		if( rtcpfd < 0 &&
			( rtcpfd = socket( PF_INET, SOCK_DGRAM, 0 ) ) < 0 )
		{
			spook_log( SL_WARN, "unable to create UDP RTCP socket: %s",
					strerror( errno ) );
			close( rtpfd );
			return -1;
		}
		if( bind( rtpfd, (struct sockaddr *)&rtpaddr,
					sizeof( rtpaddr ) ) < 0 )
		{
			if( errno == EADDRINUSE )
			{
				port += 2;
				continue;
			}
			spook_log( SL_WARN, "strange error when binding RTP socket: %s",
					strerror( errno ) );
			close( rtpfd );
			close( rtcpfd );
			return -1;
		}
		if( bind( rtcpfd, (struct sockaddr *)&rtcpaddr,
					sizeof( rtcpaddr ) ) < 0 )
		{
			if( errno == EADDRINUSE )
			{
				close( rtpfd );
				rtpfd = -1;
				port += 2;
				continue;
			}
			spook_log( SL_WARN, "strange error when binding RTCP socket: %s",
					strerror( errno ) );
			close( rtpfd );
			close( rtcpfd );
			return -1;
		}
		success = 1;
		break;
	}
	if( ! success )
	{
		spook_log( SL_WARN, "ran out of UDP RTP ports!" );
		return -1;
	}
	rtpaddr.sin_family = rtcpaddr.sin_family = AF_INET;
	rtpaddr.sin_addr = rtcpaddr.sin_addr = dest_ip;
	rtpaddr.sin_port = htons( dest_port );
	rtcpaddr.sin_port = htons( dest_port + 1 );
	if( connect( rtpfd, (struct sockaddr *)&rtpaddr,
				sizeof( rtpaddr ) ) < 0 )
	{
		spook_log( SL_WARN, "strange error when connecting RTP socket: %s",
				strerror( errno ) );
		close( rtpfd );
		close( rtcpfd );
		return -1;
	}
	if( connect( rtcpfd, (struct sockaddr *)&rtcpaddr,
				sizeof( rtcpaddr ) ) < 0 )
	{
		spook_log( SL_WARN, "strange error when connecting RTCP socket: %s",
				strerror( errno ) );
		close( rtpfd );
		close( rtcpfd );
		return -1;
	}
	i = sizeof( rtpaddr );
	if( getsockname( rtpfd, (struct sockaddr *)&rtpaddr, &i ) < 0 )
	{
		spook_log( SL_WARN, "strange error from getsockname: %s",
				strerror( errno ) );
		close( rtpfd );
		close( rtcpfd );
		return -1;
	}

    if( fcntl( rtpfd, F_SETFL, O_NONBLOCK ) < 0 )
            spook_log( SL_INFO, "error setting O_NONBLOCK on socket: %s",
                    strerror( errno ) );
    
    if( fcntl( rtcpfd, F_SETFL, O_NONBLOCK ) < 0 )
            spook_log( SL_INFO, "error setting O_NONBLOCK on socket: %s",
                    strerror( errno ) );

	ep->max_data_size = 1400; /* good guess for preventing fragmentation */
	ep->trans_type = RTP_TRANS_UDP;
	sprintf( ep->trans.udp.sdp_addr, "%s",
				inet_ntoa( rtpaddr.sin_addr ) );
	ep->trans.udp.sdp_port = ntohs( rtpaddr.sin_port );
	ep->trans.udp.rtp_fd = rtpfd;
	ep->trans.udp.rtcp_fd = rtcpfd;
	ep->trans.udp.rtp_event = add_fd_event( rtpfd, 0, 0, udp_rtp_read, ep );
	ep->trans.udp.rtcp_event =
				add_fd_event( rtcpfd, 0, 0, udp_rtcp_read, ep );

	*our_port = port;

	return 0;
}

int connect_mudp_endpoint( struct rtp_endpoint *ep,
		struct in_addr dest_ip, int dest_rtp_port, int dest_rtcp_port )
{
	struct sockaddr_in rtpaddr, rtcpaddr;
	int port, success = 0, i, max_tries, rtpfd = -1, rtcpfd = -1;
    int sndbufsize = 0xFFFF;
    
	rtpaddr.sin_family = rtcpaddr.sin_family = AF_INET;
	rtpaddr.sin_addr.s_addr = rtcpaddr.sin_addr.s_addr = g_ulLocalAddr;
	rtpaddr.sin_port = htons( dest_rtp_port );
	rtcpaddr.sin_port = htons( dest_rtcp_port );
	if( rtpfd < 0 &&
		( rtpfd = socket( PF_INET, SOCK_DGRAM, 0 ) ) < 0 )
	{
		spook_log( SL_WARN, "unable to create UDP RTP socket: %s",
				strerror( errno ) );
		return -1;
	}
	if( rtcpfd < 0 &&
		( rtcpfd = socket( PF_INET, SOCK_DGRAM, 0 ) ) < 0 )
	{
		spook_log( SL_WARN, "unable to create UDP RTCP socket: %s",
				strerror( errno ) );
		close( rtpfd );
		return -1;
	}
	if( bind( rtpfd, (struct sockaddr *)&rtpaddr,
				sizeof( rtpaddr ) ) < 0 )
	{
		spook_log( SL_WARN, "strange error when binding RTP socket: %s",
				strerror( errno ) );
		close( rtpfd );
		close( rtcpfd );
		return -1;
	}
	if( bind( rtcpfd, (struct sockaddr *)&rtcpaddr,
				sizeof( rtcpaddr ) ) < 0 )
	{			
		spook_log( SL_WARN, "strange error when binding RTCP socket: %s",
				strerror( errno ) );
		close( rtpfd );
		close( rtcpfd );
		return -1;
	}
    
	success = 1;

	if( ! success )
	{
		spook_log( SL_WARN, "ran out of UDP RTP ports!" );
		return -1;
	}
	rtpaddr.sin_family = rtcpaddr.sin_family = AF_INET;
	rtpaddr.sin_addr.s_addr = rtcpaddr.sin_addr.s_addr = htonl(dest_ip.s_addr);
	rtpaddr.sin_port = htons( dest_rtp_port );
	rtcpaddr.sin_port = htons( dest_rtcp_port );
	if( connect( rtpfd, (struct sockaddr *)&rtpaddr,
				sizeof( rtpaddr ) ) < 0 )
	{
		spook_log( SL_WARN, "strange error when connecting RTP socket %08x, %d: %s",
				rtpaddr.sin_addr.s_addr, dest_rtp_port, strerror( errno ) );
		close( rtpfd );
		close( rtcpfd );
		return -1;
	}
	if( connect( rtcpfd, (struct sockaddr *)&rtcpaddr,
				sizeof( rtcpaddr ) ) < 0 )
	{
		spook_log( SL_WARN, "strange error when connecting RTCP socket: %s",
				strerror( errno ) );
		close( rtpfd );
		close( rtcpfd );
		return -1;
	}
	i = sizeof( rtpaddr );
	if( getsockname( rtpfd, (struct sockaddr *)&rtpaddr, &i ) < 0 )
	{
		spook_log( SL_WARN, "strange error from getsockname: %s",
				strerror( errno ) );
		close( rtpfd );
		close( rtcpfd );
		return -1;
	}

    if( fcntl( rtpfd, F_SETFL, O_NONBLOCK ) < 0 )
		spook_log( SL_INFO, "error setting O_NONBLOCK on socket: %s",
				strerror( errno ) );

    if( fcntl( rtcpfd, F_SETFL, O_NONBLOCK ) < 0 )
		spook_log( SL_INFO, "error setting O_NONBLOCK on socket: %s",
				strerror( errno ) );

    if (setsockopt(rtpfd, SOL_SOCKET, SO_SNDBUF,
		   (char*)&sndbufsize, sizeof(sndbufsize)) != 0)
    {        
        spook_log( SL_INFO, "error setting SO_SNDBUF on socket: %s",
                        strerror( errno ) );
    }

	ep->max_data_size = 1400; /* good guess for preventing fragmentation */
	ep->trans_type = RTP_TRANS_MC;
    ep->trans.udp.dest_ip.s_addr = htonl(dest_ip.s_addr);
    ep->trans.udp.dest_rtp_port = htons(dest_rtp_port);
    ep->trans.udp.dest_rtcp_port = htons(dest_rtcp_port);
	sprintf( ep->trans.udp.sdp_addr, "%s", inet_ntoa(ep->trans.udp.dest_ip) );
	ep->trans.udp.sdp_port = ( dest_rtp_port );
	ep->trans.udp.rtp_fd = rtpfd;
	ep->trans.udp.rtcp_fd = rtcpfd;
	ep->trans.udp.rtp_event = add_fd_event( rtpfd, 0, 0, udp_rtp_read, ep );
	ep->trans.udp.rtcp_event =
				add_fd_event( rtcpfd, 0, 0, udp_rtcp_read, ep );
    

	return 0;
}

void connect_interleaved_endpoint( struct rtp_endpoint *ep,
		struct conn *conn, int rtp_chan, int rtcp_chan )
{
	ep->trans_type = RTP_TRANS_INTER;
	ep->trans.inter.conn = conn;
	ep->trans.inter.rtp_chan = rtp_chan;
	ep->trans.inter.rtcp_chan = rtcp_chan;
}
extern unsigned int g_ulChkTv;

int send_rtp_packet( struct rtp_endpoint *ep, struct iovec *v, int count,
			unsigned int timestamp, int marker )
{
	unsigned char rtphdr[12];
	struct msghdr mh;
	int i;
    unsigned int tStart, tNow;

    tStart = get_tickcount();
	ep->last_timestamp = ( ep->start_timestamp + timestamp * ep->tick2tsratio)
					& 0xFFFFFFFF;
    tNow = get_tickcount();
    if((tNow - tStart) > g_ulChkTv)
    {
        //printf("\r\nmultiply Error:%d!!!!\r\n", (tNow - tStart));            
    }
    tStart = tNow;

//	spook_log( SL_DEBUG, "RTP: payload %d, seq %u, time %u, marker %d",
//		ep->payload, ep->seqnum, ep->last_timestamp, marker );

	rtphdr[0] = 2 << 6; /* version */
	rtphdr[1] = ep->payload;
	if( marker ) rtphdr[1] |= 0x80;
	PUT_16( rtphdr + 2, ep->seqnum );
	PUT_32( rtphdr + 4, ep->last_timestamp );
	PUT_32( rtphdr + 8, ep->ssrc );

	v[0].iov_base = rtphdr;
	v[0].iov_len = 12;

	switch( ep->trans_type )
	{
	case RTP_TRANS_UDP:
    case RTP_TRANS_MC:
		memset( &mh, 0, sizeof( mh ) );
		mh.msg_iov = v;
		mh.msg_iovlen = count;
		if( sendmsg( ep->trans.udp.rtp_fd, &mh, 0 ) < 0 )
		{
            int nLen = 0;
            fd_set fdWrite;
            struct timeval tv;

            return -1;
/*
            FD_ZERO(&fdWrite);
            FD_SET(ep->trans.udp.rtp_fd, &fdWrite);
            tv.tv_sec = 0;
            tv.tv_usec = 20000;
            if(select(ep->trans.udp.rtp_fd + 1, NULL, &fdWrite, NULL, &tv))
            {
                if(FD_ISSET(ep->trans.udp.rtp_fd, &fdWrite))
                {
                    continue;
                }
            }
*/
            //tNow = get_tickcount();
            //if((tNow - tStart) > g_ulChkTv)
            {                
                //printf("\r\nsndmsg Error:%d, len:%d!!!!\r\n", (tNow - tStart), nLen);  
                //break;
            }
    
            for(i = 0; i < mh.msg_iovlen; i++)
            {
                nLen += mh.msg_iov[i].iov_len;
            }
			//spook_log( SL_VERBOSE, "error sending UDP RTP frame %d: %d-%s",
			//		nLen, errno, strerror( errno ) );
            break;

            if(ep->session)
    			ep->session->teardown( ep->session, ep );
			return -1;
		}
		break;
	case RTP_TRANS_INTER:
		if( interleave_send( ep->trans.inter.conn,
				ep->trans.inter.rtp_chan, v, count ) < 0 )
		{
			spook_log( SL_VERBOSE, "error sending interleaved RTP frame" );
			ep->session->teardown( ep->session, ep );
			return -1;
		}
		break;
	}

	for( i = 1; i < count; ++i ) ep->octet_count += v[i].iov_len;
	++ep->packet_count;

	if( ep->force_rtcp )
	{
		if( rtcp_send( ep ) < 0 ) return -1;
		set_event_enabled( ep->rtcp_send_event, 1 );
	}

	ep->seqnum = ( ep->seqnum + 1 ) & 0xFFFF;

	return 0;
}

/********************* GLOBAL CONFIGURATION DIRECTIVES ********************/

int config_rtprange( int num_tokens, struct token *tokens, void *d )
{
	rtp_port_start = tokens[1].v.num;
	rtp_port_end = tokens[2].v.num;

	if( rtp_port_start & 0x1 ) ++rtp_port_start;
	if( ! ( rtp_port_end & 0x1 ) ) --rtp_port_end;

	spook_log( SL_DEBUG, "RTP port range is %d-%d",
				rtp_port_start, rtp_port_end );

	if( rtp_port_end - rtp_port_start + 1 < 8 )
	{
		spook_log( SL_ERR, "at least 8 ports are needed for RTP" );
		exit( 1 );
	}

	return 0;
}
