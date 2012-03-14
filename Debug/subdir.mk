################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../access_log.c \
../audio.c \
../audio_play.c \
../c_udp_server.c \
../conf_parse.c \
../conf_scan.c \
../control.c \
../conversions.c \
../event.c \
../filter-framedrop.c \
../frame.c \
../http-auth.c \
../http.c \
../input-audio.c \
../input-video.c \
../live.c \
../log.c \
../md5.c \
../pmsg.c \
../rtp-h264.c \
../rtp-jpeg.c \
../rtp-mpeg4.c \
../rtp-rawaudio.c \
../rtp.c \
../rtsp.c \
../session.c \
../sound_conv.c \
../spook.c \
../stream.c \
../c_speak.c \
../c_rtsp.c \
../tcp.c 

OBJS += \
./access_log.o \
./audio.o \
./audio_play.o \
./c_udp_server.o \
./conf_parse.o \
./conf_scan.o \
./control.o \
./conversions.o \
./event.o \
./filter-framedrop.o \
./frame.o \
./http-auth.o \
./http.o \
./input-audio.o \
./input-video.o \
./live.o \
./log.o \
./md5.o \
./pmsg.o \
./rtp-h264.o \
./rtp-jpeg.o \
./rtp-mpeg4.o \
./rtp-rawaudio.o \
./rtp.o \
./rtsp.o \
./session.o \
./sound_conv.o \
./spook.o \
./stream.o \
./c_speak.o \
./c_rtsp.o \
./tcp.o 

C_DEPS += \
./access_log.d \
./audio.d \
./audio_play.d \
./c_udp_server.d \
./conf_parse.d \
./conf_scan.d \
./control.d \
./conversions.d \
./event.d \
./filter-framedrop.d \
./frame.d \
./http-auth.d \
./http.d \
./input-audio.d \
./input-video.d \
./live.d \
./log.d \
./md5.d \
./pmsg.d \
./rtp-h264.d \
./rtp-jpeg.d \
./rtp-mpeg4.d \
./rtp-rawaudio.d \
./rtp.d \
./rtsp.d \
./session.d \
./sound_conv.d \
./spook.d \
./stream.d \
./c_speak.d \
./c_rtsp.d \
./tcp.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-linux-gnueabi-gcc -I../ -I../include -I../enet-1.3.3/include -O0 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


