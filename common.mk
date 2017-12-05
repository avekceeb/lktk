
CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -DDEBUG=1

INSTALLDIR = build
KERNTEST = $(PWD)/kit/lktk
LUAFLAGS = -I $(PWD)/lua

INSTALL = install
INSTALL_EXEC = $(INSTALL) -m 0755
INSTALL_DATA = $(INSTALL) -m 0644
MKDIR = mkdir -p
RM = rm -f

IS_TTY = 1

ifeq ($(IS_TTY),1)
    _Y:=\\033[33m
    _R:=\\033[31m
    _N:=\\033[m
  define MESSAGE
    -@printf "$(_Y)%s$(_N)\n" "$(1)"
  endef

  define ERROR_MESSAGE
    -@printf "$(_R)%s$(_N)\n" "$(1)"
  endef

else
  define MESSAGE
    -@printf "%s\n" "$(1)"
  endef
  ERROR_MESSAGE=$(MESSAGE)
endif

### harness stuff

qemusys = qemu-system-x86_64
qemuvmhome = $(PWD)/vm
qemurootdev = /dev/sda
qemuconsole = tty1
qemukernel = $(qemuvmhome)/defkern
qemudisk = $(qemuvmhome)/defimg
qemusshport = 22222
qemusshid = $(qemuvmhome)/id_rsa

qemucmd = $(qemusys) -kernel $(qemukernel) \
  -drive file=$(qemudisk),format=raw \
  -device e1000,netdev=net0 \
  -netdev user,id=net0,hostfwd=tcp::$(qemusshport)-:22 \
  -enable-kvm -m 2G -smp 2 -daemonize -snapshot \
  -append "root=$(qemurootdev) console=$(qemuconsole)"

qemusshcmd = ssh -q -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  -o ConnectTimeout=10 -o SendEnv='' \
  root@localhost -p $(qemusshport) -i $(qemusshid)

define qemusendfile
    cat $(1) | $(qemusshcmd) "cd /root ; cat > `basename $(1)`"
endef

define qemusenddir
    tar cf - -C `dirname $(1)` `basename $(1)` \
		--owner=0 --group=0 | \
        $(qemusshcmd) "cd /root; tar xf -"
endef

linuxsrc = $(PWD)/linux


