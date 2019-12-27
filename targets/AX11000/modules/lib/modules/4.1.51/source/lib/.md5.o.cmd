cmd_lib/md5.o := /opt/toolchains//crosstools-aarch64-gcc-5.5-linux-4.1-glibc-2.26-binutils-2.28.1/usr/bin/aarch64-buildroot-linux-gnu-gcc -Wp,-MD,lib/.md5.o.d  -nostdinc -isystem /opt/toolchains/crosstools-aarch64-gcc-5.5-linux-4.1-glibc-2.26-binutils-2.28.1/lib/gcc/aarch64-buildroot-linux-gnu/5.5.0/include -I./arch/arm64/include -Iarch/arm64/include/generated/uapi -Iarch/arm64/include/generated  -Iinclude -I./arch/arm64/include/uapi -Iarch/arm64/include/generated/uapi -I./include/uapi -Iinclude/generated/uapi -include ./include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -std=gnu89 -DMULTIPLE_SSID -DSAMBA_ENABLE -DX_ST_ML -DU12H315 -DR8000 -DR7800 -DU12H334 -DAX11000 -DBCM53125 -DINCLUDE_USB_LED -DWIFI_LED_BLINKING -DIGMP_PROXY -D__CONFIG_IGMP_SNOOPING__ -DINCLUDE_L2TP -DAP_MODE -DINCLUDE_DUAL_BAND -DCONFIG_RUSSIA_IPTV -DCONFIG_KERNEL_2_6_36 -DINCLUDE_ACCESSCONTROL -DINCLUDE_DETECT_AP_MODE -DARP_PROTECTION -DVLAN_SUPPORT -DINCLULDE_2ND_5G_RADIO -DDUAL_TRI_BAND_HW_SUPPORT -DCONFIG_2ND_SWITCH -DSUPPORT_2DOT5G_WAN -DU12H335T21 -D__FOXCONN_KERNEL_PORTING__ -DCONFIG_NAT_65536_SESSION -DWW_VERSION -DBCMVISTAROUTER -DINCLUDE_QOS -DRESTART_ALL_PROCESSES -DNETGEAR_PATCH -DETEROBO -mgeneral-regs-only -fno-delete-null-pointer-checks -fno-PIE -O2 --param=allow-store-data-races=0 -DCC_HAVE_ASM_GOTO -Wframe-larger-than=2048 -fno-stack-protector -Wno-unused-but-set-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-var-tracking-assignments -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fno-stack-check -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -DBCA_HNDROUTER -g -Werror -Wfatal-errors -Wno-date-time -Wno-declaration-after-statement -Wno-switch-bool    -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(md5)"  -D"KBUILD_MODNAME=KBUILD_STR(md5)" -c -o lib/md5.o lib/md5.c

source_lib/md5.o := lib/md5.c

deps_lib/md5.o := \
  include/linux/compiler.h \
    $(wildcard include/config/sparse/rcu/pointer.h) \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
    $(wildcard include/config/bcm/kf/buzzz.h) \
    $(wildcard include/config/buzzz/func.h) \
    $(wildcard include/config/kprobes.h) \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
    $(wildcard include/config/gcov/kernel.h) \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  include/uapi/linux/types.h \
  arch/arm64/include/generated/asm/types.h \
  include/uapi/asm-generic/types.h \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  arch/arm64/include/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
    $(wildcard include/config/64bit.h) \
  include/uapi/asm-generic/bitsperlong.h \
  include/uapi/linux/posix_types.h \
  include/linux/stddef.h \
  include/uapi/linux/stddef.h \
  arch/arm64/include/uapi/asm/posix_types.h \
  include/uapi/asm-generic/posix_types.h \
  include/linux/export.h \
    $(wildcard include/config/have/underscore/symbol/prefix.h) \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/unused/symbols.h) \
  include/linux/cryptohash.h \

lib/md5.o: $(deps_lib/md5.o)

$(deps_lib/md5.o):
