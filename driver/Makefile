obj-m += leetmouse.o
leetmouse-objs := accel.o driver.o

# Detect architecture
ARCH := $(shell uname -m)

# Architecture-specific flags
ifeq ($(ARCH), ppc64le)
    EXTRA_CFLAGS += -D__ppc64le__
endif

#ccflags-y += -mhard-float

all:
	cp -n config.sample.h config.h
