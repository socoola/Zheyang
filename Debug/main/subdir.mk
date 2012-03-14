################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
MAIN_O_SRCS += \
../main/main.o \
../main/t_audio.o \
../main/t_speak.o \
../main/t_rtsp.o \
../main/t_video.o \
../main/t_wlchk.o 

MAIN_C_SRCS += \
../main/main.c \
../main/t_audio.c \
../main/t_speak.c \
../main/t_rtsp.c \
../main/t_video.c \
../main/t_wlchk.c 

MAIN_OBJS += \
./main/main.o \
./main/t_audio.o \
./main/t_speak.o \
./main/t_rtsp.o \
./main/t_video.o \
./main/t_wlchk.o 

MAIN_C_DEPS += \
./main/main.d \
./main/t_audio.d \
./main/t_speak.d \
./main/t_rtsp.d \
./main/t_video.d \
./main/t_wlchk.d 

# Each subdirectory must supply rules for building sources it contributes
main/%.o: ../main/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-linux-gnueabi-gcc -g -I../ -I../include -I/usr/local/arm-linux-2.6.28/linux-2.6.28-fa/include -I/usr/local/arm-linux-2.6.28/module/include -I../enet-1.3.3/include -O0 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


