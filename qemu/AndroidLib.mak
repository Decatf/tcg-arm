include $(CLEAR_VARS)

LOCAL_CFLAGS :=
LOCAL_C_INCLUDES := $(LOCAL_PATH)
qemu_src_files :=
qemu_src_files_exclude :=

FILE_LIST_UTIL := $(wildcard $(LOCAL_PATH)/util/*.c)
qemu_src_files += $(FILE_LIST_UTIL:$(LOCAL_PATH)/%=%)
qemu_src_files_exclude += \
	util/buffer.c \
	util/coroutine-gthread.c \
	util/coroutine-sigaltstack.c \
	util/coroutine-ucontext.c \
	util/coroutine-win32.c \
	util/event_notifier-win32.c \
	util/oslib-win32.c \
	util/qemu-sockets.c \
	util/qemu-coroutine.c \
	util/qemu-coroutine-io.c \
	util/qemu-thread-win32.c \
	util/qemu-coroutine-lock.c \
	util/qemu-coroutine-sleep.c \

FILE_LIST_QOBJECT := $(wildcard $(LOCAL_PATH)/qobject/*.c)
qemu_src_files += $(FILE_LIST_QOBJECT:$(LOCAL_PATH)/%=%)

FILE_LIST_QAPI := $(wildcard $(LOCAL_PATH)/qapi/*.c)
qemu_src_files += $(FILE_LIST_QAPI:$(LOCAL_PATH)/%=%)

FILE_LIST_HW_CORE := $(wildcard $(LOCAL_PATH)/hw/core/*.c)
qemu_src_files += $(FILE_LIST_HW_CORE:$(LOCAL_PATH)/%=%)
qemu_src_files_exclude += \
	hw/core/bus.c \
	hw/core/loader.c \
	hw/core/ptimer.c \
	hw/core/sysbus.c \
	hw/core/machine.c \
	hw/core/register.c \
	hw/core/empty_slot.c \
	hw/core/null-machine.c \
	hw/core/platform-bus.c \
	hw/core/fw-path-provider.c \

FILE_LIST_CRYPTO := $(wildcard $(LOCAL_PATH)/crypto/*.c)
qemu_src_files += $(FILE_LIST_CRYPTO:$(LOCAL_PATH)/%=%)
qemu_src_files_exclude += \
	crypto/block.c \
	crypto/block-luks.c \
	crypto/block-qcow.c \
	crypto/cipher.c \
	crypto/cipher-builtin.c \
	crypto/cipher-gcrypt.c \
	crypto/cipher-nettle.c \
	crypto/cipher-nettle.c \
	crypto/hash-gcrypt.c \
	crypto/hash-nettle.c \
	crypto/ivgen.c \
	crypto/ivgen-essiv.c \
	crypto/pbkdf-gcrypt.c \
	crypto/pbkdf-nettle.c \
	crypto/random-gcrypt.c \
	crypto/random-gnutls.c \
	crypto/secret.c \

qemu_src_files += \
	qom/container.c \
	qom/cpu.c \
	qom/object.c \
	qom/object_interfaces.c \
	qom/qom-qobject.c \

qemu_src_files += \
	stubs/cpus.c \
	stubs/fd-register.c \
	stubs/fdset-add-fd.c  \
	stubs/fdset-get-fd.c \
	stubs/fdset-find-fd.c \
	stubs/fdset-remove-fd.c \
	stubs/iothread-lock.c \
	stubs/mon-is-qmp.c \
	stubs/notify-event.c \
	stubs/machine-init-done.c \
	stubs/vmstate.c \
	stubs/sysbus.c \
	stubs/qtest.c \
	stubs/replay.c \
	stubs/iohandler.c \
	stubs/set-fd-handler.c \
	stubs/trace-control.c \
	stubs/is-daemonized.c \

qemu_src_files += \
	trace/control.c \
	trace/generated-events.c \

#
# TCG
#
qemu_src_files += \
	tcg-runtime.c \
	tcg/tcg.c \
	tcg/tcg-op.c \
	tcg/optimize.c \
	tcg/tcg-common.c \

#
# TCI
#
CONFIG_TCG_INTERPRETER := 0
ifeq ($(CONFIG_TCG_INTERPRETER), 1)
	LOCAL_CFLAGS += -DCONFIG_TCG_INTERPRETER
	# required for ARM guest
	LOCAL_CFLAGS += -DTCG_TARGET_CALL_ALIGN_ARGS
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/tcg/tci
	qemu_src_files += \
		tci.c \
		disas/tci.c
else
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/tcg/arm
endif

qemu_src_files += fpu/softfloat.c

#
## target-arm
#
qemu_src_files += \
	target-arm/helper.c \
	target-arm/crypto_helper.c \
	target-arm/iwmmxt_helper.c \
	target-arm/op_helper.c \
	target-arm/neon_helper.c \
	target-arm/translate.c \
	target-arm/cpu.c
qemu_src_files_exclude += \
	target-arm/cpu64.c \
	target-arm/kvm64.c \
	target-arm/gdbstub64.c \
	target-arm/translate-a64.c

qemu_src_files += \
	disas.c \
	disas/arm.c \

qemu_src_files += \
	qapi-event.c \
	qapi-types.c \
	qapi-visit.c \
	qemu-timer.c \
	gdbstub.c \
	cpu-exec-common.c \
	translate-common.c \
	translate-all.c \
	linux-user/mmap.c \

qemu_src_files += \
	tcg-arm/exec.c \
	tcg-arm/cpus.c \
	tcg-arm/cpu-exec.c \
	tcg-arm/vl.c \
	tcg-arm/main.c \
	tcg-arm/tcg-arm.c

LOCAL_SRC_FILES := $(filter-out $(qemu_src_files_exclude), $(qemu_src_files))

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/trace
LOCAL_C_INCLUDES += $(LOCAL_PATH)/tcg

LOCAL_C_INCLUDES += $(LOCAL_PATH)/target-arm

LOCAL_C_INCLUDES += $(LOCAL_PATH)/linux-user
LOCAL_C_INCLUDES += $(LOCAL_PATH)/linux-user/arm
LOCAL_C_INCLUDES += $(LOCAL_PATH)/linux-user/host/arm

LOCAL_CFLAGS += -DNEED_CPU_H
LOCAL_CFLAGS += -DCONFIG_USER_ONLY
LOCAL_CFLAGS += -DANDROID_ARMEMU

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/tcg-arm

LOCAL_CFLAGS += \
	-Wno-unused-variable \
	-Wno-unused-parameter \
	-Wno-sign-compare \
	-Wno-pointer-arith \
	-Wno-missing-field-initializers

LOCAL_CFLAGS += -mcpu=cortex-a9 -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16-fp16

LOCAL_CFLAGS += \
	-std=gnu99 \
	-Wno-psabi \
	-O3 \
	-fpic \
	-ffunction-sections \
	-fomit-frame-pointer \
	-fno-strict-aliasing \
	-fforce-addr \
	-ffast-math \
	-foptimize-sibling-calls \
	-funsafe-math-optimizations \
	-fno-stack-protector \
	-DNDEBUG

LOCAL_CLANG := true
ifeq ($(LOCAL_CLANG), false)
LOCAL_CFLAGS += \
	-fstrength-reduce \
	-finline-limit=99999
endif

LOCAL_CFLAGS += -fvisibility=hidden

# LOCAL/_CFLAGS += \
# 	-O0
# LOCAL_STRIP_MODULE := keep_symbols

LOCAL_SHARED_LIBRARIES:= libz libcutils liblog
LOCAL_STATIC_LIBRARIES:= libglib-2.0 libgthread-2.0 libgobject-2.0 libgmodule-2.0
LOCAL_MODULE    := libtcg_arm


include $(BUILD_SHARED_LIBRARY)