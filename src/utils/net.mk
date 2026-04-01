IFACE:=xjos

BR0:=/sys/class/net/br0
TAPS:=	/sys/class/net/tap0 \
		/sys/class/net/tap1 \
		/sys/class/net/tap2 \

.SECONDARY: $(TAPS) $(BR0)

# 网桥 IP 地址
BNAME:=br0
IP0:=192.168.239.111
MAC0:=5a:5a:5a:5a:5a:22
GATEWAY:=192.168.239.2
# Keep host L3 on br0 optional; default disabled to avoid route/ARP conflicts
BR0_L3 ?= 0

.PHONY: netup

# Always enforce bridge membership before qemu start.
# /sys/class/net based targets are kept for compatibility.
netup:
	@if ! ip link show $(BNAME) >/dev/null 2>&1; then \
		sudo ip link add $(BNAME) type bridge; \
	fi
	sudo ip link set $(BNAME) type bridge ageing_time 0

	sudo ip link set $(IFACE) down || true
	sudo ip addr flush dev $(IFACE) || true
	sudo ip link set $(IFACE) nomaster || true
	sudo ip link set $(IFACE) master $(BNAME)
	sudo ip link set $(IFACE) up

	sudo ip link set dev $(BNAME) address $(MAC0)
	sudo ip link set $(BNAME) up

	@# Avoid ARP flux when multiple interfaces are in same L2 subnet.
	sudo sysctl -w net.ipv4.conf.all.arp_ignore=1 >/dev/null
	sudo sysctl -w net.ipv4.conf.all.arp_announce=2 >/dev/null
	sudo sysctl -w net.ipv4.conf.default.arp_ignore=1 >/dev/null
	sudo sysctl -w net.ipv4.conf.default.arp_announce=2 >/dev/null

	@if [ "$(BR0_L3)" = "1" ]; then \
		sudo ip addr replace $(IP0)/24 brd + dev $(BNAME); \
		sudo ip route replace default via $(GATEWAY) dev $(BNAME) proto static metric 10000; \
	else \
		sudo ip addr flush dev $(BNAME) scope global || true; \
	fi

	@for tap in tap0 tap1 tap2; do \
		sudo ip tuntap add mode tap $$tap 2>/dev/null || true; \
		sudo ip link set $$tap nomaster 2>/dev/null || true; \
		sudo ip link set $$tap master $(BNAME); \
		sudo ip link set dev $$tap up; \
	done

$(BR0): netup
	@:

br0: netup
	@:

/sys/class/net/tap%: netup
	@:

tap%: netup
	@:
