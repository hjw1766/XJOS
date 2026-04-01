.PHONY: bochs
bochs: $(IMAGES)
	bochs -q -f ../bochs/bochsrc -unlock

# AUDIO_OUT := $(BUILD_DIR)/sb16_capture.wav

QEMU:= qemu-system-i386
QEMU+= -m 32M
# QEMU+= -audiodev pa,id=hda	# audio dev				* temp ban audio
# QEMU+= -machine pcspk-audiodev=hda	# pc speaker dev
# QEMU += -audiodev wav,id=snd,path=$(AUDIO_OUT)
# QEMU += -machine pcspk-audiodev=snd
# QEMU += -device sb16,audiodev=snd

QEMU+= -rtc base=localtime	# set rtc to localtime
QEMU+= -drive file=$(BUILD_DIR)/master.img,if=ide,index=0,media=disk,format=raw # master hard disk
QEMU+= -drive file=$(BUILD_DIR)/slave.img,if=ide,index=1,media=disk,format=raw # slave hard disk
# QEMU+= -drive file=$(BUILD_DIR)/floppya.img,if=floppy,index=0,media=disk,format=raw # software floppy disk
QEMU+= -chardev stdio,mux=on,id=com1 # char dev 1
# QEMU+= -chardev vc,mux=on,id=com1 # char dev 1
# QEMU+= -chardev vc,mux=on,id=com2 # char dev 2
#QEMU+= -chardev udp,mux=on,id=com2,port=7777,ipv4=on # char dev 2
QEMU+= -serial chardev:com1 # COM 1
#QEMU+= -serial chardev:com2 # COM 2
QEMU+= -netdev tap,id=eth0,ifname=tap0,script=no,downscript=no # 网络设备
QEMU+= -device e1000,netdev=eth0,mac=5A:5A:5A:5A:5A:33 # 网卡 e1000

QEMU_DISK:=-boot c

QEMU_DEBUG:= -s -S

.PHONY: database
database:
	@echo "Updating compile_commands.json via compiledb..."
	@python3 -m compiledb -n $(MAKE) all || (echo "Error: compiledb not found. Try 'pip install compiledb'"; exit 1)
	@echo "Update complete."

.PHONY: qemu
qemu: $(IMAGES) netup
	$(QEMU) $(QEMU_DISK)

.PHONY: qemug
qemug: $(IMAGES) netup
	$(QEMU) $(QEMU_DISK) $(QEMU_DEBUG)

# vmware vmdk image
$(BUILD_DIR)/master.vmdk: $(BUILD_DIR)/master.img
	qemu-img convert -O vmdk $< $@

.PHONY: vmdk
vmdk: $(BUILD_DIR)/master.vmdk