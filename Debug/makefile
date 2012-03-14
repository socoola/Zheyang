################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include enet-1.3.3/subdir.mk
-include enet-1.3.3/.libs/subdir.mk
-include wireless/subdir.mk
-include wireless/.libs/subdir.mk
-include main/subdir.mk
-include main/.libs/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: udp_svr udp_svr_share

rtsplib.a: $(OBJS) $(USER_OBJS)
	arm-none-linux-gnueabi-ar rcs rtsplib.a $(OBJS) $(USER_OBJS)

libZheyang.so: $(OBJS) $(USER_OBJS)
	arm-none-linux-gnueabi-gcc -shared -o libZheyang.so $(OBJS) $(USER_OBJS)
	cp libZheyang.so ../

udp_svr_share: libZheyang.so $(MAIN_OBJS) $(MAIN_USER_OBJS)
	arm-none-linux-gnueabi-gcc -L../ -o "udp_svr_share" $(MAIN_OBJS) $(MAIN_USER_OBJS) -lZheyang $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Tool invocations
udp_svr: rtsplib.a $(MAIN_OBJS) $(MAIN_USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	arm-none-linux-gnueabi-gcc -L../ -o "udp_svr" $(MAIN_OBJS) $(MAIN_USER_OBJS) rtsplib.a $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) $(MAIN_OBJS) $(MAIN_USER_OBJS) $(MAIN_C_DEPS) udp_svr rtsplib.a libZheyang.so udp_svr_share
	-@echo ' '

post-build:
	-cp -f udp_svr /tftpboot && cp -f ../spook.conf /tftpboot
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY: post-build

-include ../makefile.targets
