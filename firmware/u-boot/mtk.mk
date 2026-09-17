U_BOOT_DIR             := $(shell pwd)

.PHONY: all

all:

	@source ../../../../../BDP_Generic/setenv.sh && source ./link.sh
	if [ "`grep mt8530_base_config ./link.sh`" != "" ]; then \
	  echo "mt8530_base_config has been configed"; \
	else \
	  echo "mt8530_base_config has not been configed then config it"; \
	 make -j ${JOBS} -C ${U_BOOT_DIR} mt8530_base_config O=../../../build/u-boot/u-boot-2009.08 && echo "#mt8530_base_config success" >> link.sh; \
	fi

	 make -j ${JOBS} -C ${U_BOOT_DIR} O=../../../build/u-boot/u-boot-2009.08
	 @cp -f ../../../build/u-boot/u-boot-2009.08/u-boot.bin ../../../../res/u-boot-8530.bin
