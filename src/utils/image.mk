$(BUILD_DIR)/master.img: $(BUILD_DIR)/boot/boot.bin \
	$(BUILD_DIR)/boot/loader.bin \
	$(BUILD_DIR)/system.bin \
	$(BUILD_DIR)/system.map \
	$(SRC_DIR)/utils/master.sfdisk

# create 16M disk image
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@

# write boot.bin
	dd if=$(BUILD_DIR)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc

# write loader.bin
	dd if=$(BUILD_DIR)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc

# test system.bin < 100k
	test -n "$$(find $(BUILD_DIR)/system.bin -size -100k)"

# write system.bin
	dd if=$(BUILD_DIR)/system.bin of=$@ bs=512 count=200 seek=10 conv=notrunc

# part
	sfdisk $@ < $(SRC_DIR)/utils/master.sfdisk

# mount dev
	sudo losetup /dev/loop0 --partscan $@

# minix
	sudo mkfs.minix -1 -n 14 /dev/loop0p1

# mount
	sudo mount /dev/loop0p1 /mnt

# root
	sudo chown ${USER} /mnt

# mkdir
	mkdir -p /mnt/dev
	mkdir -p /mnt/mnt

# file
	echo "hello xjos!!!, from root dir file..." > /mnt/hello.txt

# Fix gip bug
	sudo chown -R 1000:0 /mnt
# un file sys
	sudo umount /mnt

# un dev
	sudo losetup -d /dev/loop0

$(BUILD_DIR)/slave.img: \
	$(SRC_DIR)/utils/slave.sfdisk

# create 32M disk image
	yes | bximage -q -hd=32 -func=create -sectsize=512 -imgmode=flat $@

# part
	sfdisk $@ < $(SRC_DIR)/utils/slave.sfdisk

	sudo losetup /dev/loop0 --partscan $@

	sudo mkfs.minix -1 -n 14 /dev/loop0p1

	sudo mount /dev/loop0p1 /mnt

	sudo chown ${USER} /mnt 

	echo "slave root dir file..." > /mnt/hello.txt

	sudo chown -R 1000:0 /mnt

	sudo umount /mnt

	sudo losetup -d /dev/loop0

.PHONY: mount0
mount0: $(BUILD_DIR)/master.img
	sudo losetup /dev/loop0 --partscan $<
	sudo mount /dev/loop0p1 /mnt
	sudo chown ${USER} /mnt

.PHONY: umount0
umount0: /dev/loop0
	-sudo umount /mnt
	-sudo losetup -d $<

.PHONY: mount1
mount1: $(BUILD_DIR)/slave.img
	sudo losetup /dev/loop0 --partscan $<
	sudo mount /dev/loop0p1 /mnt
	sudo chown ${USER} /mnt
	
.PHONY: umount1
umount1: /dev/loop1
	-sudo umount /mnt
	-sudo losetup -d $<

IMAGES:= $(BUILD_DIR)/master.img \
	$(BUILD_DIR)/slave.img

image: $(IMAGES)