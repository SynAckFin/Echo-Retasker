
include src/dkms.conf
PACKAGE_NAME ?= Unknown
PACKAGE_VERSION ?= 0.0

DTSRC += echo-2mic.dts
DTOVERLAY = $(DTSRC:.dts=.dtbo)
DTC_FLAGS = -b 0 -Wno-unit_address_vs_reg -I dts -O dtb
OVERLAYDIR = /boot/overlays


%.dtbo: %.dts
	dtc -@ $(DTC_FLAGS) -o $@ $<

all:	$(DTOVERLAY)

install: install-driver install-overlay

install-driver:
	@if ! [ "$(shell id -u)" = 0 ];then \
		echo "You are not root, run this target as root please";\
		exit 1;\
	fi
	-dkms remove -m $(PACKAGE_NAME) -v $(PACKAGE_VERSION) --all
	make -C src/ clean
	rm -rf /usr/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)
	cp -a src/ /usr/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)
	dkms add -m $(PACKAGE_NAME) -v $(PACKAGE_VERSION)
	dkms install -m $(PACKAGE_NAME) -v $(PACKAGE_VERSION)

install-overlay: $(DTOVERLAY)
	@if ! [ "$(shell id -u)" = 0 ];then \
		echo "You are not root, run this target as root please";\
		exit 1;\
	fi
	cp $^ $(OVERLAYDIR)/
	-@for ov in $(^:.dtbo=); do \
	  dtoverlay $$ov; \
	done

uninstall: uninstall-driver uninstall-overlay

uninstall-driver:
	@if ! [ "$(shell id -u)" = 0 ];then \
		echo "You are not root, run this target as root please";\
		exit 1;\
	fi
	-dkms remove -m $(PACKAGE_NAME) -v $(PACKAGE_VERSION) --all

uninstall-overlay: $(DTOVERLAY)
	@if ! [ "$(shell id -u)" = 0 ];then \
		echo "You are not root, run this target as root please";\
		exit 1;\
	fi
	-@for ov in $(^:.dtbo=); do \
	  dtoverlay -r $$ov; \
	done
	rm -f $(addprefix $(OVERLAYDIR)/,$^)

clean:
	make -C src/ clean
	rm -f *.dtbo

