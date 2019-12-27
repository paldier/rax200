	.cpu cortex-a53+fp+simd+crc
	.file	"bounds.c"
// GNU C89 (Buildroot 2017.11.1) version 5.5.0 (aarch64-buildroot-linux-gnu)
//	compiled by GNU C version 4.1.2 20080704 (Red Hat 4.1.2-44), GMP version 6.1.2, MPFR version 3.1.6, MPC version 1.0.3
// GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
// options passed:  -nostdinc -I ./arch/arm64/include
// -I arch/arm64/include/generated/uapi -I arch/arm64/include/generated
// -I include -I ./arch/arm64/include/uapi
// -I arch/arm64/include/generated/uapi -I ./include/uapi
// -I include/generated/uapi
// -isysroot /opt/toolchains/crosstools-aarch64-gcc-5.5-linux-4.1-glibc-2.26-binutils-2.28.1/aarch64-buildroot-linux-gnu/sysroot
// -D __KERNEL__ -D MULTIPLE_SSID -D SAMBA_ENABLE -D X_ST_ML -D U12H315
// -D R8000 -D R7800 -D U12H334 -D AX11000 -D BCM53125 -D INCLUDE_USB_LED
// -D WIFI_LED_BLINKING -D IGMP_PROXY -D __CONFIG_IGMP_SNOOPING__
// -D INCLUDE_L2TP -D AP_MODE -D INCLUDE_DUAL_BAND -D CONFIG_RUSSIA_IPTV
// -D CONFIG_KERNEL_2_6_36 -D INCLUDE_ACCESSCONTROL
// -D INCLUDE_DETECT_AP_MODE -D ARP_PROTECTION -D VLAN_SUPPORT
// -D INCLULDE_2ND_5G_RADIO -D DUAL_TRI_BAND_HW_SUPPORT
// -D CONFIG_2ND_SWITCH -D SUPPORT_2DOT5G_WAN -D U12H335T21
// -D __FOXCONN_KERNEL_PORTING__ -D CONFIG_NAT_65536_SESSION -D WW_VERSION
// -D BCMVISTAROUTER -D INCLUDE_QOS -D RESTART_ALL_PROCESSES
// -D NETGEAR_PATCH -D ETEROBO -D CC_HAVE_ASM_GOTO -D BCA_HNDROUTER
// -D KBUILD_STR(s)=#s -D KBUILD_BASENAME=KBUILD_STR(bounds)
// -D KBUILD_MODNAME=KBUILD_STR(bounds)
// -isystem /opt/toolchains/crosstools-aarch64-gcc-5.5-linux-4.1-glibc-2.26-binutils-2.28.1/lib/gcc/aarch64-buildroot-linux-gnu/5.5.0/include
// -include ./include/linux/kconfig.h -MD kernel/.bounds.s.d
// kernel/bounds.c -mlittle-endian -mgeneral-regs-only -mcpu=cortex-a53
// -mabi=lp64 -auxbase-strip kernel/bounds.s -Os -O2 -Wall -Wundef
// -Wstrict-prototypes -Wno-trigraphs -Werror=implicit-function-declaration
// -Wno-format-security -Wframe-larger-than=2048
// -Wno-unused-but-set-variable -Wdeclaration-after-statement
// -Wno-pointer-sign -Werror=implicit-int -Werror=strict-prototypes
// -Werror=date-time -std=gnu90 -fno-strict-aliasing -fno-common
// -fno-delete-null-pointer-checks -fno-PIE -fno-stack-protector
// -fno-omit-frame-pointer -fno-optimize-sibling-calls
// -fno-var-tracking-assignments -fno-strict-overflow -fstack-check=no
// -fconserve-stack -fverbose-asm --param allow-store-data-races=0
// options enabled:  -faggressive-loop-optimizations -falign-labels
// -fauto-inc-dec -fbranch-count-reg -fcaller-saves
// -fchkp-check-incomplete-type -fchkp-check-read -fchkp-check-write
// -fchkp-instrument-calls -fchkp-narrow-bounds -fchkp-optimize
// -fchkp-store-bounds -fchkp-use-static-bounds
// -fchkp-use-static-const-bounds -fchkp-use-wrappers
// -fcombine-stack-adjustments -fcompare-elim -fcprop-registers
// -fcrossjumping -fcse-follow-jumps -fdefer-pop -fdevirtualize
// -fdevirtualize-speculatively -fdwarf2-cfi-asm -fearly-inlining
// -feliminate-unused-debug-types -fexpensive-optimizations
// -fforward-propagate -ffunction-cse -fgcse -fgcse-lm -fgnu-runtime
// -fgnu-unique -fguess-branch-probability -fhoist-adjacent-loads -fident
// -fif-conversion -fif-conversion2 -findirect-inlining -finline
// -finline-atomics -finline-functions-called-once -finline-small-functions
// -fipa-cp -fipa-cp-alignment -fipa-icf -fipa-icf-functions
// -fipa-icf-variables -fipa-profile -fipa-pure-const -fipa-ra
// -fipa-reference -fipa-sra -fira-hoist-pressure -fira-share-save-slots
// -fira-share-spill-slots -fisolate-erroneous-paths-dereference -fivopts
// -fkeep-static-consts -fleading-underscore -flifetime-dse -flra-remat
// -flto-odr-type-merging -fmath-errno -fmerge-constants
// -fmerge-debug-strings -fmove-loop-invariants -fomit-frame-pointer
// -foptimize-strlen -fpartial-inlining -fpeephole -fpeephole2
// -fprefetch-loop-arrays -free -freg-struct-return -freorder-blocks
// -freorder-functions -frerun-cse-after-loop
// -fsched-critical-path-heuristic -fsched-dep-count-heuristic
// -fsched-group-heuristic -fsched-interblock -fsched-last-insn-heuristic
// -fsched-pressure -fsched-rank-heuristic -fsched-spec
// -fsched-spec-insn-heuristic -fsched-stalled-insns-dep -fschedule-fusion
// -fschedule-insns -fschedule-insns2 -fsection-anchors
// -fsemantic-interposition -fshow-column -fshrink-wrap -fsigned-zeros
// -fsplit-ivs-in-unroller -fsplit-wide-types -fssa-phiopt -fstdarg-opt
// -fstrict-volatile-bitfields -fsync-libcalls -fthread-jumps
// -ftoplevel-reorder -ftrapping-math -ftree-bit-ccp
// -ftree-builtin-call-dce -ftree-ccp -ftree-ch -ftree-coalesce-vars
// -ftree-copy-prop -ftree-copyrename -ftree-cselim -ftree-dce
// -ftree-dominator-opts -ftree-dse -ftree-forwprop -ftree-fre
// -ftree-loop-if-convert -ftree-loop-im -ftree-loop-ivcanon
// -ftree-loop-optimize -ftree-parallelize-loops= -ftree-phiprop -ftree-pre
// -ftree-pta -ftree-reassoc -ftree-scev-cprop -ftree-sink -ftree-slsr
// -ftree-sra -ftree-switch-conversion -ftree-tail-merge -ftree-ter
// -ftree-vrp -funit-at-a-time -fverbose-asm -fzero-initialized-in-bss
// -mgeneral-regs-only -mglibc -mlittle-endian -momit-leaf-frame-pointer

	.text
	.align	2
	.p2align 3,,7
	.global	foo
	.type	foo, %function
foo:
#APP
// 18 "kernel/bounds.c" 1
	
->NR_PAGEFLAGS 22 __NR_PAGEFLAGS	//
// 0 "" 2
// 19 "kernel/bounds.c" 1
	
->MAX_NR_ZONES 3 __MAX_NR_ZONES	//
// 0 "" 2
// 21 "kernel/bounds.c" 1
	
->NR_CPUS_BITS 2 ilog2(CONFIG_NR_CPUS)	//
// 0 "" 2
// 23 "kernel/bounds.c" 1
	
->SPINLOCK_SIZE 4 sizeof(spinlock_t)	//
// 0 "" 2
#NO_APP
	ret
	.size	foo, .-foo
	.ident	"GCC: (Buildroot 2017.11.1) 5.5.0"
	.section	.note.GNU-stack,"",%progbits
