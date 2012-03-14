################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../wireless/splitstr.o \
../wireless/iwconfig.o \
../wireless/iwpriv.o \
../wireless/iwlib.o \
../wireless/wireless_check.o 

C_SRCS += \
../wireless/splitstr.c \
../wireless/iwconfig.c \
../wireless/iwpriv.c \
../wireless/iwlib.c \
../wireless/wireless_check.c 

OBJS += \
./wireless/splitstr.o \
./wireless/iwconfig.o \
./wireless/iwpriv.o \
./wireless/iwlib.o \
./wireless/wireless_check.o 

C_DEPS += \
./wireless/splitstr.d \
./wireless/iwconfig.d \
./wireless/iwpriv.d \
./wireless/iwlib.d \
./wireless/wireless_check.d 

# Each subdirectory must supply rules for building sources it contributes
wireless/%.o: ../wireless/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-linux-gnueabi-gcc -g -I../ -I../include -I/usr/local/arm-linux-2.6.28/linux-2.6.28-fa/include -I/usr/local/arm-linux-2.6.28/module/include -I../enet-1.3.3/include -O0 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


