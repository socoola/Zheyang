include 8126.config
CFLAGS += -I./include -I./enet-1.3.3/include

OBJEXT = o
TARGET = udp_svr
OBJS = main.o t_video.o t_audio.o c_udp_server.o conf_parse.$(OBJEXT) conf_scan.$(OBJEXT) \
	conversions.$(OBJEXT) log.$(OBJEXT) pmsg.$(OBJEXT) \
	md5.$(OBJEXT) event.$(OBJEXT) filter-framedrop.$(OBJEXT) \
	frame.$(OBJEXT) control.$(OBJEXT) tcp.$(OBJEXT) http.$(OBJEXT) \
	audio.$(OBJEXT) rtp.$(OBJEXT) session.$(OBJEXT) rtsp.$(OBJEXT) \
	spook.$(OBJEXT) stream.$(OBJEXT) access_log.$(OBJEXT) \
	http-auth.$(OBJEXT) live.$(OBJEXT) rtp-mpeg4.$(OBJEXT) \
	rtp-rawaudio.$(OBJEXT) \
	rtp-h264.$(OBJEXT) rtp-jpeg.$(OBJEXT) input-video.$(OBJEXT) \
	input-audio.$(OBJEXT)  \
	sound_conv.$(OBJEXT)
EXTRALIB = -lcomponent -lalpum -lplatform -lgeneral -lssl -lcrypto -lpthread

all : $(TARGET)
	
$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(EXTRALIB)
	$(STRIP) $(TARGET)
	chmod 777 $(TARGET)
	cp -f $(TARGET) $(TFTP)
	cp -f spook.conf $(TFTP)
	
clean :
	/bin/rm -f *.o
	/bin/rm -f $(TARGET)
