#
# setup environment
#
cpu_count=`cat /proc/cpuinfo |grep processor|wc -l`
if [ -z ${MAKE} ]; then
    export MAKE="make -j $cpu_count"
fi
echo "MAKE=$MAKE"
# config uboot
U_BOOT_DIR=.

${MAKE} -C ${U_BOOT_DIR} mt8530_base_config O=../../../build/u-boot/u-boot-2009.08