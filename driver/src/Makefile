
uname_r=$(shell uname -r)

MODSRC += tlv320adc3xxx.c
obj-m = $(MODSRC:%.c=snd-soc-%.o)

$(foreach S,$(MODSRC), \
	$(eval $(S:%.c=snd-soc-%-objs) = $(S:.c=.o)))

all:
	make -C /lib/modules/$(uname_r)/build M=$(PWD) modules

clean:
	rm -f *.order *.symvers *.ko *.mod* *.o

.PHONY: all clean install
