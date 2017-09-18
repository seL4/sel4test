#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

# Targets
TARGETS := $(notdir $(SOURCE_DIR)).bin

# Make sure this symbol stays around as we don't reference this, but
# whoever loads us will
LDFLAGS += -u __vsyscall_ptr

# Source files required to build the target
CFILES :=  $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))
CFILES +=  $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/tests/*.c))
CFILES +=  $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/arch/${ARCH}/*.c))
CFILES +=  $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/arch/${ARCH}/tests/*.c))
CFILES +=  $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/plat/${PLAT}/*.c))
CXXFILES :=  $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.cxx))
CXXFILES +=  $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/tests/*.cxx))

ASMFILES :=  $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/arch/${ARCH}/tests/*.S))

HDRFILES := $(wildcard $(SOURCE_DIR)/include/*) \
    $(wildcard ${SOURCE_DIR}/arch/${ARCH}/*) \
    $(wildcard ${SOURCE_DIR}/sel4_arch/${SEL4_ARCH}/*)

# Libraries
LIBS := sel4 c cpio sel4muslcsys utils sel4utils sel4allocman sel4simple sel4test sel4testsupport elf sel4platsupport platsupport sel4vspace sel4simple-default sel4debug sel4sync sel4serialserver

include $(SEL4_COMMON)/common.mk
