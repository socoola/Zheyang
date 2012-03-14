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
#include "config.h"
#include "iwlib.h"

typedef struct tagApInfo
{
unsigned char bssid[6];
char ssid[33];
unsigned char type;
unsigned char rssi;
unsigned char channel;
unsigned char rate;
unsigned char auth;
unsigned char encrypt;
} __attribute__((packed, aligned(1))) RT_AP_INFO;

#define TIME_LESS(a, b) ((long)((a) - (b)) < 0)

static bool g_bChkWirelessLink = 0;
extern bool g_bStopSend;
unsigned int g_nSignalQualityThreshold = 70, g_nSignalDiffThreshold = 1, g_nCountry = 2;
static char g_szSSID[64] = "zycamera";
static char g_szAddress[32] = "192.168.2.170";

extern unsigned int g_nDroprateThreshold;

/************************* DISPLAY ROUTINES **************************/

/*------------------------------------------------------------------*/
/*
 * Get wireless informations & config from the device driver
 * We will call all the classical wireless ioctl on the driver through
 * the socket to know what is supported and to get the settings...
 */
static int
get_info(int			skfd,
	 char *			ifname,
	 struct wireless_info *	info)
{
  struct iwreq		wrq;

  memset((char *) info, 0, sizeof(struct wireless_info));

  /* Get basic information */
  if(iw_get_basic_config(skfd, ifname, &(info->b)) < 0)
    {
      /* If no wireless name : no wireless extensions */
      /* But let's check if the interface exists at all */
      struct ifreq ifr;

      strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
      if(ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
	return(-ENODEV);
      else
	return(-ENOTSUP);
    }

  /* Get ranges */
  if(iw_get_range_info(skfd, ifname, &(info->range)) >= 0)
    info->has_range = 1;

  /* Get sensitivity */
  if(iw_get_ext(skfd, ifname, SIOCGIWSENS, &wrq) >= 0)
    {
      info->has_sens = 1;
      memcpy(&(info->sens), &(wrq.u.sens), sizeof(iwparam));
    }

  /* Get AP address */
  if(iw_get_ext(skfd, ifname, SIOCGIWAP, &wrq) >= 0)
    {
      info->has_ap_addr = 1;
      memcpy(&(info->ap_addr), &(wrq.u.ap_addr), sizeof (sockaddr));
    }

  /* Get NickName */
  wrq.u.essid.pointer = (caddr_t) info->nickname;
  wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
  wrq.u.essid.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWNICKN, &wrq) >= 0)
    if(wrq.u.data.length > 1)
      info->has_nickname = 1;

  /* Get bit rate */
  if(iw_get_ext(skfd, ifname, SIOCGIWRATE, &wrq) >= 0)
    {
      info->has_bitrate = 1;
      memcpy(&(info->bitrate), &(wrq.u.bitrate), sizeof(iwparam));
    }

  /* Get RTS threshold */
  if(iw_get_ext(skfd, ifname, SIOCGIWRTS, &wrq) >= 0)
    {
      info->has_rts = 1;
      memcpy(&(info->rts), &(wrq.u.rts), sizeof(iwparam));
    }

  /* Get fragmentation threshold */
  if(iw_get_ext(skfd, ifname, SIOCGIWFRAG, &wrq) >= 0)
    {
      info->has_frag = 1;
      memcpy(&(info->frag), &(wrq.u.frag), sizeof(iwparam));
    }

  /* Get Power Management settings */
  wrq.u.power.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWPOWER, &wrq) >= 0)
    {
      info->has_power = 1;
      memcpy(&(info->power), &(wrq.u.power), sizeof(iwparam));
    }

  if((info->has_range) && (info->range.we_version_compiled > 9))
    {
      /* Get Transmit Power */
      if(iw_get_ext(skfd, ifname, SIOCGIWTXPOW, &wrq) >= 0)
	{
	  info->has_txpower = 1;
	  memcpy(&(info->txpower), &(wrq.u.txpower), sizeof(iwparam));
	}
    }

  if((info->has_range) && (info->range.we_version_compiled > 10))
    {
      /* Get retry limit/lifetime */
      if(iw_get_ext(skfd, ifname, SIOCGIWRETRY, &wrq) >= 0)
	{
	  info->has_retry = 1;
	  memcpy(&(info->retry), &(wrq.u.retry), sizeof(iwparam));
	}
    }

  /* Get stats */
  if(iw_get_stats(skfd, ifname, &(info->stats),
		  &info->range, info->has_range) >= 0)
    {
      info->has_stats = 1;
    }

#ifdef DISPLAY_WPA
  /* Note : currently disabled to not bloat iwconfig output. Also,
   * if does not make total sense to display parameters that we
   * don't allow (yet) to configure.
   * For now, use iwlist instead... Jean II */

  /* Get WPA/802.1x/802.11i security parameters */
  if((info->has_range) && (info->range.we_version_compiled > 17))
    {
      wrq.u.param.flags = IW_AUTH_KEY_MGMT;
      if(iw_get_ext(skfd, ifname, SIOCGIWAUTH, &wrq) >= 0)
	{
	  info->has_auth_key_mgmt = 1;
	  info->auth_key_mgmt = wrq.u.param.value;
	}

      wrq.u.param.flags = IW_AUTH_CIPHER_PAIRWISE;
      if(iw_get_ext(skfd, ifname, SIOCGIWAUTH, &wrq) >= 0)
	{
	  info->has_auth_cipher_pairwise = 1;
	  info->auth_cipher_pairwise = wrq.u.param.value;
	}

      wrq.u.param.flags = IW_AUTH_CIPHER_GROUP;
      if(iw_get_ext(skfd, ifname, SIOCGIWAUTH, &wrq) >= 0)
	{
	  info->has_auth_cipher_group = 1;
	  info->auth_cipher_group = wrq.u.param.value;
	}
    }
#endif

  return(0);
}

static int get_wifi_signal_quality(int skfd, char *ifname)
{
    int quality = 0;
    bool bConnected = false;
    struct wireless_info info;
    char ap_addr_buf[64];

    memset(&info, 0, sizeof(info));    
    if(get_info(skfd, WIRELESS_IFNAME, &info) == 0)
    {
        if((info.has_ap_addr) && (info.b.has_essid))
        {
            bConnected = true;
        }

        if(bConnected && info.has_stats)
        {
            /* Just do it...
            * The old way to detect dBm require both the range and a non-null
            * level (which confuse the test). The new way can deal with level of 0
            * because it does an explicit test on the flag. */
            iwqual* qual = &info.stats.qual;
            if(info.has_range && ((qual->level != 0) || (qual->updated & IW_QUAL_DBM)))
            {
                /* Deal with quality : always a relative value */
                if(!(qual->updated & IW_QUAL_QUAL_INVALID))
                {
                    quality = qual->qual;
                }
            }
        }
    }

    if(info.has_ap_addr)
    {
        iw_ether_ntop((const struct ether_addr *)info.ap_addr.sa_data, ap_addr_buf);
        spook_log( SL_INFO, "Current AP %s signal quality:%d", ap_addr_buf, quality);
    }
    else
    {
        spook_log( SL_INFO, "This wireless is disconnected");
    }

    return quality;
}

/*------------------------------------------------------------------*/
/*
 * Perform a scanning on one device
 */
static int
get_bestap_by_ssid(int skfd, char* ifname, unsigned char szCurApAddr[6], char *ssid, int diff_threshold)
{
  struct iwreq		wrq;
  unsigned char *	buffer = NULL;		/* Results */
  int			buflen = IW_SCAN_MAX_DATA; /* Min for compat WE<17 */
  struct iw_range	range;
  int			has_range;
  struct timeval	tv;				/* Select timeout */
  int			timeout = 15000000;		/* 15s */
  int ret = 0;
  char ap_addr_buf[64];

  /* Get range stuff */
  has_range = (iw_get_range_info(skfd, ifname, &range) >= 0);

  /* Check if the interface could support scanning. */
  if((!has_range) || (range.we_version_compiled < 14))
    {
      fprintf(stderr, "%-8.16s  Interface doesn't support scanning.\n\n",
	      ifname);
      return(-1);
    }

  /* Init timeout value -> 250ms*/
  tv.tv_sec = 0;
  tv.tv_usec = 250000;

  /*
   * Here we should look at the command line args and set the IW_SCAN_ flags
   * properly
   */
  wrq.u.data.pointer = NULL;		/* Later */
  wrq.u.data.flags = 0;
  wrq.u.data.length = 0;

  /* Initiate Scanning */
  if(iw_set_ext(skfd, ifname, SIOCSIWSCAN, &wrq) < 0)
    {
      if(errno != EPERM)
	{
	  fprintf(stderr, "%-8.16s  Interface doesn't support scanning : %s\n\n",
		  ifname, strerror(errno));
	  return(-1);
	}
      /* If we don't have the permission to initiate the scan, we may
       * still have permission to read left-over results.
       * But, don't wait !!! */
#if 0
      /* Not cool, it display for non wireless interfaces... */
      fprintf(stderr, "%-8.16s  (Could not trigger scanning, just reading left-over results)\n", ifname);
#endif
      tv.tv_usec = 0;
    }
    timeout -= tv.tv_usec;

    /* Forever */
    while(1)
    {
        fd_set		rfds;		/* File descriptors for select */
        int		last_fd;	/* Last fd */
        int		ret;

        /* Guess what ? We must re-generate rfds each time */
        FD_ZERO(&rfds);
        last_fd = -1;

        /* In here, add the rtnetlink fd in the list */

        /* Wait until something happens */
        ret = select(last_fd + 1, &rfds, NULL, NULL, &tv);

        /* Check if there was an error */
        if(ret < 0)
        {
            if(errno == EAGAIN || errno == EINTR)
            continue;
            fprintf(stderr, "Unhandled signal - exiting...\n");
            return(-1);
        }

        /* Check if there was a timeout */
        if(ret == 0)
        {
          unsigned char *	newbuf;

realloc:
          /* (Re)allocate the buffer - realloc(NULL, len) == malloc(len) */
          newbuf = realloc(buffer, buflen);
          if(newbuf == NULL)
            {
              if(buffer)
        	free(buffer);
              fprintf(stderr, "%s: Allocation failed\n", __FUNCTION__);
              return(-1);
            }
          buffer = newbuf;

          /* Try to read the results */
          wrq.u.data.pointer = buffer;
          wrq.u.data.flags = 0;
          wrq.u.data.length = buflen;
          if(iw_get_ext(skfd, ifname, SIOCGIWSCAN, &wrq) < 0)
            {
              /* Check if buffer was too small (WE-17 only) */
              if((errno == E2BIG) && (range.we_version_compiled > 16))
        	{
        	  /* Some driver may return very large scan results, either
        	   * because there are many cells, or because they have many
        	   * large elements in cells (like IWEVCUSTOM). Most will
        	   * only need the regular sized buffer. We now use a dynamic
        	   * allocation of the buffer to satisfy everybody. Of course,
        	   * as we don't know in advance the size of the array, we try
        	   * various increasing sizes. Jean II */

        	  /* Check if the driver gave us any hints. */
        	  if(wrq.u.data.length > buflen)
        	    buflen = wrq.u.data.length;
        	  else
        	    buflen *= 2;

        	  /* Try again */
        	  goto realloc;
        	}

              /* Check if results not available yet */
              if(errno == EAGAIN)
        	{
        	  /* Restart timer for only 100ms*/
        	  tv.tv_sec = 0;
        	  tv.tv_usec = 100000;
        	  timeout -= tv.tv_usec;
        	  if(timeout > 0)
        	    continue;	/* Try again later */
        	}

              /* Bad error */
              free(buffer);
              fprintf(stderr, "%-8.16s  Failed to read scan data : %s\n\n",
        	      ifname, strerror(errno));
              return(-2);
            }
          else
            /* We have the results, go to process them */
            break;
        }

      /* In here, check if event and event type
       * if scan event, read results. All errors bad & no reset timeout */
    }

    if(wrq.u.data.length)
    {
        int ap_num, best_ap_num, best_singal, cur_ap_signal;
        RT_AP_INFO *rt_ap = (RT_AP_INFO *)buffer;

        best_singal = cur_ap_signal = -256;
        for(ap_num = 0; ap_num < wrq.u.data.length/sizeof(RT_AP_INFO); ++ap_num) 
        {            
            if(strcmp(ssid, rt_ap[ap_num].ssid) != 0)
            {
                continue;
            }

            iw_ether_ntop((const struct ether_addr *)rt_ap[ap_num].bssid, ap_addr_buf);
            spook_log( SL_INFO, "found %s:%d", ap_addr_buf, rt_ap[ap_num].rssi);
            
            if(memcmp(rt_ap[ap_num].bssid, szCurApAddr, 6) == 0)
            {
                cur_ap_signal = rt_ap[ap_num].rssi;
            }
            
            if(rt_ap[ap_num].rssi >= best_singal)
            {
                best_ap_num = ap_num;
                best_singal = rt_ap[ap_num].rssi;
            }
        }

        if(best_singal > (cur_ap_signal + diff_threshold))
        {
            iw_ether_ntop((const struct ether_addr *)rt_ap[best_ap_num].bssid, ap_addr_buf);
            spook_log( SL_INFO, "Find a better AP %s, quality %d", ap_addr_buf, best_singal);
            memcpy(szCurApAddr, rt_ap[best_ap_num].bssid, 6);
            ret = 1;            
        }
        else
        {
            iw_ether_ntop((const struct ether_addr *)szCurApAddr, ap_addr_buf);
            spook_log( SL_INFO, "current AP %s is the best, quality %d", ap_addr_buf, best_singal);
        }
    }

    free(buffer);
    return ret;
}

/*------------------------------------------------------------------*/
/*
 * Display the AP Address if possible
 */
static int
get_wifi_addr(int skfd,
	 const char *ifname,
	 unsigned char addr_buf[6])
{
    struct iwreq wrq;

    /* Get AP Address */
    if(iw_get_ext(skfd, ifname, SIOCGIWAP, &wrq) < 0)
    return(-1);

    /* Print */
    memcpy(addr_buf, wrq.u.ap_addr.sa_data, 6);
    return(0);
}
//connect2ap
///mnt/user/iwpriv eth1 set NetworkType=Infra
///mnt/user/iwpriv eth1 set AuthMode=Open
///mnt/user/iwpriv eth1 set EncrypType=None
///mnt/user/iwpriv eth1 set SSID=""
///mnt/user/iwpriv eth1 set SSID="$1"
bool connect2ssid(char *ssid)
{
    char szCmd[128];

    sprintf(szCmd, "%s/iwpriv %s set NetworkType=Infra", WORK_DICTORY, WIRELESS_IFNAME);
    if(iwpriv(szCmd) != 0)
        spook_log( SL_WARN, "excute cmd %d failed", szCmd);
    sprintf(szCmd, "%s/iwpriv %s set AuthMode=Open", WORK_DICTORY, WIRELESS_IFNAME);
    if(iwpriv(szCmd) != 0)
        spook_log( SL_WARN, "excute cmd %d failed", szCmd);
    sprintf(szCmd, "%s/iwpriv %s set EncrypType=None", WORK_DICTORY, WIRELESS_IFNAME);
    if(iwpriv(szCmd) != 0)
        spook_log( SL_WARN, "excute cmd %d failed", szCmd);
    
    sprintf(szCmd, "%s/iwpriv %s set SSID=", WORK_DICTORY, WIRELESS_IFNAME);
    if(iwpriv(szCmd) != 0)
        spook_log( SL_WARN, "excute cmd %d failed", szCmd);
    sprintf(szCmd, "%s/iwpriv %s set SSID=%s", WORK_DICTORY, WIRELESS_IFNAME, ssid);
    if(iwpriv(szCmd) != 0)
        spook_log( SL_WARN, "excute cmd %d failed", szCmd);

    //system("/mnt/user/connect2ap zycamera");
    spook_log( SL_INFO, "connect to SSID:%s", ssid);

    return true;
}

bool connect2newap(unsigned char ap_addr[6])
{
    char ap_addr_buf[64];
    char szCmd[128];

    iw_ether_ntop((const struct ether_addr *)ap_addr, ap_addr_buf);
    sprintf(szCmd, "%s//iwconfig %s ap %s", WORK_DICTORY, WIRELESS_IFNAME, ap_addr_buf);
    if(iwconfig(szCmd) != 0)
        spook_log( SL_WARN, "excute cmd %d failed", szCmd);
    //system(szCmd);
    spook_log( SL_INFO, "switch to AP:%s", ap_addr_buf);

    return true;
}

void check_wireless_link()
{
    int skfd, quality, ret, delay, nCurChkTimes, nMaxChkTimes;
    bool bConnected, bConnect2ssid, bConnect2NewAp;
    unsigned int nNextChkTime;
    struct wireless_info info;
    unsigned char ap_addr[6];
    struct timeval	tv;				/* Select timeout */    

    /* Create a channel to the NET kernel. */
    if((skfd = iw_sockets_open()) < 0)
    {
      perror("socket");
      exit(-1);
    }

    nCurChkTimes = 0;
    nMaxChkTimes = 4;
    nNextChkTime = get_tickcount();
    while(1)
    {
        if((g_bStopSend || g_bChkWirelessLink) && 
            TIME_LESS(nNextChkTime, get_tickcount()))
        {
            bConnect2ssid = bConnect2NewAp = false;
            delay = 0;
            // STEP 1: get wireless link connectioness. if connected goto STEP2;
            // otherwise, means we are in process of connecting or disconnected.
            quality = get_wifi_signal_quality(skfd, WIRELESS_IFNAME);            
            if(quality == 0)
            {
                // not connected
                if(g_bStopSend)
                {
                    nCurChkTimes++;
                    if(nCurChkTimes >= nMaxChkTimes)
                    {
                        spook_log( SL_INFO, "Current AP is not connected in 2s");
                        bConnect2ssid = true;
                    }
                    else
                    {
                        delay = 50;
                    }
                }
                else
                {
                    bConnect2ssid = true;
                }
            }            
            // STEP 2: get wireless link signal quality
            else if(quality >= g_nSignalQualityThreshold)
            {
                // signal quality is good
                nCurChkTimes = 0;
                g_bStopSend = false;      // stop sending until signal is good          
                delay = 100;
            }
            else
            {
                // signal quality is low
                if(g_bStopSend)
                {
                    nCurChkTimes++;
                    if(nCurChkTimes >= nMaxChkTimes)
                    {
                        spook_log( SL_INFO, "Current AP's signal is not stable in 2s");
                        //bConnect2ssid = true;
                    }
                    else
                    {
                        delay = 50;
                    }
                }
                else
                {
                    //bConnect2ssid = true;
                }
            }

            if(delay)
            {
                nNextChkTime = get_tickcount() + delay;
                continue;
            }

            nCurChkTimes = 0;            
            // STEP 3: check all ap, select a best one and compare with current one
            if(quality && get_wifi_addr(skfd, WIRELESS_IFNAME, ap_addr) == 0)
            {
                spook_log( SL_INFO, "Search for better AP");
                ret = get_bestap_by_ssid(skfd, 
                    WIRELESS_IFNAME, ap_addr, g_szSSID, g_nSignalDiffThreshold);  
                if(ret > 0)
                {
                    bConnect2ssid = true; 
                    bConnect2NewAp = true;
                }
            }
            else
            {
                bConnect2ssid = true;
            }
            
            // STEP 4: switch to the best AP and stop sending.
            if(bConnect2ssid)
            {
                connect2ssid(g_szSSID);
                g_bStopSend = true;    // stop sending when connect 2 SSID
            }

            if(bConnect2NewAp)
            {
                connect2newap(ap_addr);
                g_bStopSend = true;    // stop sending when connect to new AP
                nNextChkTime = get_tickcount() + 200;
            }
            else
            {
                //g_bStopSend = false;
                nNextChkTime = get_tickcount() + 100;
            }
        }
        else
        {
            // delay and check again
            tv.tv_sec = 0;
            tv.tv_usec = 100000;

            ret = select(skfd + 1, NULL, NULL, NULL, &tv);
            
        }
    }

    iw_sockets_close(skfd);
}

static void * wireless_check_thread_function(void * arg);
/*
static void * wireless_check_thread_function1(void * arg)
{
    char szCmd[128];
    
    spook_log( SL_INFO, "Begin wireless check thread");   
    
    sprintf(szCmd, "echo [Default]>/etc/RT2870STA.dat");
    system(szCmd);
    sprintf(szCmd, "echo CountryRegion=%d>>/etc/RT2870STA.dat", g_nCountry);
    system(szCmd);    
    sprintf(szCmd, "echo SSID=\"%s\">>/etc/RT2870STA.dat", g_szSSID);
    system(szCmd); 
    sprintf(szCmd, "echo NetworkType=Infra>>/etc/RT2870STA.dat");
    system(szCmd);
    sprintf(szCmd, "echo AuthMode=OPEN>>/etc/RT2870STA.dat");
    system(szCmd);
    sprintf(szCmd, "echo EncrypType=NONE>>/etc/RT2870STA.dat");
    system(szCmd);
    system("insmod /lib/modules/rt3070sta.ko");
    system("cp /mnt/user/libiw.so.28 /lib");
    sprintf(szCmd, "ifconfig %s %s up", WIRELESS_IFNAME, g_szAddress);
    system(szCmd);
    //@TODO:write connect2ap
    check_wireless_link();

    return NULL;
}
*/



int config_wifi_country( int num_tokens, struct token *tokens, void *d )
{
	int country;

	country = tokens[1].v.num;

	if( country < 0 || country > 7 )
	{
		spook_log( SL_ERR, "invalid wifi country %d", country );
		return -1;
	}

    g_nCountry = country;

	return 0;
}

int config_droprate( int num_tokens, struct token *tokens, void *d )
{
	int droprate;

	droprate = tokens[1].v.num;

	if( droprate <= 0 || droprate > 255 )
	{
		spook_log( SL_ERR, "invalid droprate %d", droprate );
		return -1;
	}

    g_nDroprateThreshold = droprate;

	return 0;
}

int config_wifi_signal_quality( int num_tokens, struct token *tokens, void *d )
{
	int quality;

	quality = tokens[1].v.num;

	if( quality < 0 || quality > 100 )
	{
		spook_log( SL_ERR, "invalid wifi signal quality %d", quality );
		return -1;
	}

    g_nSignalQualityThreshold = quality;

	return 0;
}

int config_wifi_signal_diff( int num_tokens, struct token *tokens, void *d )
{
	int diff;

	diff = tokens[1].v.num;

	if( diff < 0 || diff > 255 )
	{
		spook_log( SL_ERR, "invalid wifi signal diff %d", diff );
		return -1;
	}

    g_nSignalDiffThreshold = diff;

	return 0;
}

int config_wifi_ssid( int num_tokens, struct token *tokens, void *d )
{
	char *ssid;
    int ssid_len;

	ssid = tokens[1].v.str;
    ssid_len = strlen(ssid);

	if( ssid_len <= 0 || ssid_len >= sizeof(g_szSSID))
	{
		spook_log( SL_ERR, "invalid wifi ssid %s", ssid );
		return -1;
	}

    strncpy(g_szSSID, ssid, sizeof(g_szSSID) - 1);
    g_szSSID[sizeof(g_szSSID) - 1] = 0;

	return 0;
}

int config_wifi_address( int num_tokens, struct token *tokens, void *d )
{
	char *address;

	address = tokens[1].v.str;
	if(inet_addr(address) == INADDR_NONE)
	{
        spook_log( SL_ERR, "invalid wifi address %s", address );
		return -1;
	}

    strncpy(g_szAddress, address, sizeof(g_szAddress) - 1);
    g_szAddress[sizeof(g_szAddress) - 1] = 0;

	return 0;
}

void begin_wireless_check()
{
    g_bChkWirelessLink = true;
}

void end_wireless_check()
{
    g_bChkWirelessLink = false;
}

static int skfd;
static unsigned int nNextChkTime;
static bool g_init_wireless = false;
bool c_init_wireless_check(bool init_wireless)
{
    char *tmp;
    char szCmd[128];
    
    spook_log( SL_INFO, "Begin wireless check thread");   
/*
    g_nCountry = c_get_param_int(PARAM_WIFI_COUNTRY);
    tmp = c_get_param_str(PARAM_WIFI_SSID);
    strncpy(g_szSSID, tmp, sizeof(g_szSSID));
    free(tmp);
    g_nDroprateThreshold = c_get_param_int(PARAM_DROPRATE);
    g_nSignalQualityThreshold = c_get_param_int(PARAM_SIGNAL_QUALITY);
    g_nSignalDiffThreshold = c_get_param_int(PARAM_SIGNAL_DIFF);
  */
    g_init_wireless = init_wireless;
    system("cp /mnt/user/libiw.so.28 /lib");
    if(init_wireless)
    {    
        sprintf(szCmd, "echo [Default]>/etc/RT2870STA.dat");
        system(szCmd);
        sprintf(szCmd, "echo CountryRegion=%d>>/etc/RT2870STA.dat", g_nCountry);
        system(szCmd);    
        sprintf(szCmd, "echo SSID=\"%s\">>/etc/RT2870STA.dat", g_szSSID);
        system(szCmd); 
        sprintf(szCmd, "echo NetworkType=Infra>>/etc/RT2870STA.dat");
        system(szCmd);
        sprintf(szCmd, "echo AuthMode=OPEN>>/etc/RT2870STA.dat");
        system(szCmd);
        sprintf(szCmd, "echo EncrypType=NONE>>/etc/RT2870STA.dat");
        system(szCmd);
        system("insmod /lib/modules/rt3070sta.ko");
        //
        sprintf(szCmd, "ifconfig %s %s up", WIRELESS_IFNAME, g_szAddress);
        system(szCmd);
    }

    /* Create a channel to the NET kernel. */
    if((skfd = iw_sockets_open()) < 0)
    {
      perror("socket");
      exit(-1);
    }

    nNextChkTime = get_tickcount();
}

bool c_do_wireless_check_handler()
{
    static int nCurChkTimes = 0;
    static int nMaxChkTimes = 4;
    
    static bool bConnect2ssid, bConnect2NewAp;
    int delay, quality,ret;
    struct wireless_info info;
    unsigned char ap_addr[6];
    struct timeval	tv;				/* Select timeout */  

    if((g_bStopSend || g_bChkWirelessLink) && 
            TIME_LESS(nNextChkTime, get_tickcount()))
    {
        spook_log( SL_INFO, "Begin wireless checking");
        bConnect2ssid = bConnect2NewAp = false;
        delay = 0;
        // STEP 1: get wireless link connectioness. if connected goto STEP2;
        // otherwise, means we are in process of connecting or disconnected.
        quality = get_wifi_signal_quality(skfd, WIRELESS_IFNAME);            
        if(quality == 0)
        {
            // not connected
            if(g_bStopSend)
            {
                nCurChkTimes++;
                if(nCurChkTimes >= nMaxChkTimes)
                {
                    spook_log( SL_INFO, "Current AP is not connected in 2s");
                    bConnect2ssid = true;
                }
                else
                {
                    delay = 100;
                }
            }
            else
            {
                bConnect2ssid = true;
            }
        }            
        // STEP 2: get wireless link signal quality
        else if(quality >= g_nSignalQualityThreshold)
        {
            // signal quality is good
            nCurChkTimes = 0;
            g_bStopSend = false;      // stop sending until signal is good          
            delay = 100;
        }
        else
        {
            // signal quality is low
            if(g_bStopSend)
            {
                nCurChkTimes++;
                if(nCurChkTimes >= nMaxChkTimes)
                {
                    spook_log( SL_INFO, "Current AP's signal is not stable in 2s");
                    //bConnect2ssid = true;
                }
                else
                {
                    delay = 50;
                }
            }
            else
            {
                //bConnect2ssid = true;
            }
        }

        if(delay)
        {
            nNextChkTime = get_tickcount() + delay;
            return;
        }

        nCurChkTimes = 0;            
        // STEP 3: check all ap, select a best one and compare with current one
        if(quality && get_wifi_addr(skfd, WIRELESS_IFNAME, ap_addr) == 0)
        {
            spook_log( SL_INFO, "Search for better AP");
            ret = get_bestap_by_ssid(skfd, 
                WIRELESS_IFNAME, ap_addr, g_szSSID, g_nSignalDiffThreshold);  
            if(ret > 0)
            {
                bConnect2ssid = true; 
                bConnect2NewAp = true;
            }
        }
        else
        {
            bConnect2ssid = true;
        }
        
        // STEP 4: switch to the best AP and stop sending.
        if(bConnect2ssid)
        {
            connect2ssid(g_szSSID);
            g_bStopSend = true;    // stop sending when connect 2 SSID
        }

        if(bConnect2NewAp)
        {
            connect2newap(ap_addr);
            g_bStopSend = true;    // stop sending when connect to new AP
            nNextChkTime = get_tickcount() + 200;
        }
        else
        {
            //g_bStopSend = false;
            nNextChkTime = get_tickcount() + 100;
        }
    }
    else if(g_init_wireless)
    {
        // delay and check again
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        ret = select(skfd + 1, NULL, NULL, NULL, &tv);
        
    }

    return true;
}

void c_deinit_wireless_check()
{
    iw_sockets_close(skfd);
}


