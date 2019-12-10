#! /bin/sh

PATH=$PATH:/usr/arm-picozed-linux-gnueabihf/bin
SYSROOT=/usr/arm-picozed-linux-gnueabihf/arm-picozed-linux-gnueabihf/sysroot/
./bootstrap
./configure --host=arm-picozed-linux-gnueabihf \
--disable-dummy \
--disable-ftdi \
--disable-stlink \
--disable-ti-icdi \
--disable-ulink \
--disable-usb-blaster-2 \
--disable-ft232r \
--disable-vsllink \
--disable-xds110 \
--disable-osbdm \
--disable-opendous \
--disable-aice \
--disable-usbprog \
--disable-rlink \
--disable-armjtagew \
--disable-cmsis-dap \
--disable-kitprog \
--disable-usb-blaster \
--disable-presto \
--disable-openjtag \
--disable-jlink \
--disable-parport \
--disable-parport-ppdev \
--disable-parport-giveio \
--disable-jtag_vpi \
--disable-amtjtagaccel \
--disable-zy1000-master \
--disable-zy1000 \
--disable-ioutil \
--disable-ep93xx \
--disable-at91rm9200 \
--disable-bcm2835gpio \
--disable-imx_gpio \
--enable-mmap_gpio \
--disable-gw16012 \
--disable-oocd_trace \
--disable-buspirate \
--disable-sysfsgpio \
--disable-minidriver-dummy
make -j8

outdir=$1
mkdir -p $outdir
mkdir -p $outdir/interface
mkdir -p $outdir/target
cp -v src/openocd $outdir
cp -v tcl/mem_helper.tcl $outdir
cp -v tcl/interface/mmap_gpio.cfg $outdir/interface
cp -v -rf tcl/interface/mmap_gpio $outdir/interface/mmap_gpio
cp -v tcl/target/psoc5lp.cfg $outdir/target
cp -v tcl/target/swj-dp.tcl $outdir/target