include common.mk

SUBDIRS = dmesg-util lua kit lua-posix-api tests

.PHONY: all posix $(SUBDIRS) $(KERNTEST) selftest kernel qemu test

ALLTESTS = $(wildcard tests/*.lua)

### BUILD TARGETS ###

all: $(KERNTEST) install

$(KERNTEST): kit 

dmesg: dmesg-util

selftest: install
	$(call MESSAGE,"test")
	cd $(INSTALLDIR) ; ./lktk sanity-test.lua

$(SUBDIRS):
	$(call MESSAGE,"$@")
	-@$(MAKE) -C $@
	#-@$(MAKE) -C $@ WITHOUT_READLINE=1

clean:
	$(call MESSAGE,"clean")
	-@$(foreach d,$(SUBDIRS),$(MAKE) -C $(d) clean;)

install: $(KERNTEST) $(INSTALLDIR)
	$(call MESSAGE,"install")
	-@$(RM) $(INSTALLDIR)/*
	-@$(INSTALL_EXEC) $(KERNTEST) $(INSTALLDIR)/lktk
	-@$(foreach f,$(ALLTESTS),$(INSTALL_DATA) $(f) $(subst tests,$(INSTALLDIR),$(f));)

$(INSTALLDIR):
	-@$(MKDIR) $@

### TEST RUN HARNESS ###

qemustart:
	-@$(qemucmd)
	-@$(qemusshcmd) 'uname -r'

qemu: qemustart
	-@$(call qemusenddir,$(INSTALLDIR))

test:
	$(call MESSAGE,\"not implemented yet: $@\")

kernel:
	$(call MESSAGE,\"not implemented yet: $@\")


help:
	-@echo "make qemustart qemudisk=/path-to-img.raw qemurootdev=/dev/sda1"
