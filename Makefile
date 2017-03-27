MODULES += modules/kmod-basic
MODULES += modules/kmod-clarg
MODULES += modules/kmod-gpio_inpirq
MODULES += modules/kmod-gpio_inpthrd
MODULES += modules/kmod-gpio_outptask
MODULES += modules/kmod-gpio_output
MODULES += modules/kmod-gpio_outptimer
# MODULES += modules/kmod-pdev
MODULES += modules/kmod-tasklet

all: pack

.PHONY: build
build: build-kernel build-modules

.PHONY: clean
clean: clean-kernel clean-modules
	if [ -f rpi-modtest-sysroot.tar.bz2 ]; then rm rpi-modtest-sysroot.tar.bz2; fi

.PHONY: distclean
distclean: clean distclean-kernel distclean-modules

.PHONY: proper
proper: distclean
	if [ -d ./tools ]; then rm -fr ./tools; fi
	if [ -d ./linux ]; then rm -fr ./linux; fi

.PHONY: install
install: build install-kernel install-modules

clone-tools:
	if ! [ -d ./tools ]; then git clone https://github.com/raspberrypi/tools; fi

clone-kernel:
	if ! [ -d ./linux ]; then git clone https://github.com/raspberrypi/linux.git linux; fi
	if ! [ -f ./linux/localversion ]; then echo "wendlers" > ./linux/localversion; fi

build-kernel: clone-tools clone-kernel
	if ! [ -f ./linux/.config ]; then (cd ./linux && make bcm2709_defconfig); fi
	(cd ./linux && make -j4)

build-modules:
	for i in $(MODULES); do (cd $$i && make); done

clean-kernel:
	(cd ./linux && make clean)

clean-modules:
	for i in $(MODULES); do (cd $$i && make clean); done

distclean-kernel:
	(cd ./linux && make distclean)

distclean-modules:
	if [ -d ./sysroot ]; then rm -r ./sysroot; fi

install-kernel:
	(cd ./linux && make INSTALL_MOD_PATH=../sysroot modules_install)
	mkdir -p ./sysroot/boot/overlays
	(cd ./linux && scripts/mkknlimg arch/arm/boot/zImage \
		../sysroot/boot/$(KERNEL).img)
	(cd ./linux && cp arch/arm/boot/dts/*.dtb ../sysroot/boot/.)
	(cd ./linux && cp arch/arm/boot/dts/overlays/*.dtb* ../sysroot/boot/overlays/.)
	(cd ./linux && cp arch/arm/boot/dts/overlays/README ../sysroot/boot/overlays/.)

install-modules:
	for i in $(MODULES); do (cd $$i && make modules_install); done

install-usrspc:
	for i in $(USRSPC); do (cd $$i && make install); done

pack: install
	(cd ./sysroot && tar -jcvf ../rpi-modtest-sysroot.tar.bz2 *)
