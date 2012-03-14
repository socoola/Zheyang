################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../enet-1.3.3/callbacks.o \
../enet-1.3.3/compress.o \
../enet-1.3.3/host.o \
../enet-1.3.3/list.o \
../enet-1.3.3/packet.o \
../enet-1.3.3/peer.o \
../enet-1.3.3/protocol.o \
../enet-1.3.3/unix.o \
../enet-1.3.3/win32.o 

C_SRCS += \
../enet-1.3.3/callbacks.c \
../enet-1.3.3/compress.c \
../enet-1.3.3/host.c \
../enet-1.3.3/list.c \
../enet-1.3.3/packet.c \
../enet-1.3.3/peer.c \
../enet-1.3.3/protocol.c \
../enet-1.3.3/unix.c 

OBJS += \
./enet-1.3.3/callbacks.o \
./enet-1.3.3/compress.o \
./enet-1.3.3/host.o \
./enet-1.3.3/list.o \
./enet-1.3.3/packet.o \
./enet-1.3.3/peer.o \
./enet-1.3.3/protocol.o \
./enet-1.3.3/unix.o 

C_DEPS += \
./enet-1.3.3/callbacks.d \
./enet-1.3.3/compress.d \
./enet-1.3.3/host.d \
./enet-1.3.3/list.d \
./enet-1.3.3/packet.d \
./enet-1.3.3/peer.d \
./enet-1.3.3/protocol.d \
./enet-1.3.3/unix.d 


# Each subdirectory must supply rules for building sources it contributes
enet-1.3.3/%.o: ../enet-1.3.3/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-linux-gnueabi-gcc -I../ -I../include -I/usr/local/arm-linux-2.6.28/linux-2.6.28-fa/include -I/usr/local/arm-linux-2.6.28/module/include -I../enet-1.3.3/include -O0 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


