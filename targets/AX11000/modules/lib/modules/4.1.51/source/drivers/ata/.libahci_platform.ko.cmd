cmd_drivers/ata/libahci_platform.ko := /opt/toolchains//crosstools-aarch64-gcc-5.5-linux-4.1-glibc-2.26-binutils-2.28.1/usr/bin/aarch64-buildroot-linux-gnu-ld -EL -r  -T ./scripts/module-common.lds --build-id  -o drivers/ata/libahci_platform.ko drivers/ata/libahci_platform.o drivers/ata/libahci_platform.mod.o