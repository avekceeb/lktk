// autogenerated by syzkaller (http://github.com/google/syzkaller)

// 9705596d08ac87c18aee32cc97f2783b7d14624e (4.12-rc6+)
// https://groups.google.com/forum/#!topic/syzkaller/VtONA6oTiio
/*
commit 9705596d08ac87c18aee32cc97f2783b7d14624e
Merge: 865be78022e9 949bdfed4b0f
Author: Linus Torvalds <torvalds@linux-foundation.org>
Date:   Tue Jun 20 11:02:29 2017 +0800

    Merge tag 'clk-fixes-for-linus' of git://git.kernel.org/pub/scm/linux/kernel/git/clk/linux
    
    Pull clk fixes from Stephen Boyd:
     "One build fix for an Amlogic clk driver and a handful of Allwinner clk
      driver fixes for some DT bindings and a randconfig build error that
      all came in this merge window"
    
    * tag 'clk-fixes-for-linus' of git://git.kernel.org/pub/scm/linux/kernel/git/clk/linux:
      clk: sunxi-ng: a64: Export PLL_PERIPH0 clock for the PRCM
      clk: sunxi-ng: h3: Export PLL_PERIPH0 clock for the PRCM
      dt-bindings: clock: sunxi-ccu: Add pll-periph to PRCM's needed clocks
      clk: sunxi-ng: sun5i: Fix ahb_bist_clk definition
      clk: sunxi-ng: enable SUNXI_CCU_MP for PRCM
      clk: meson: gxbb: fix build error without RESET_CONTROLLER
      clk: sunxi-ng: v3s: Fix usb otg device reset bit
      clk: sunxi-ng: a31: Correct lcd1-ch1 clock register offset
*/
#ifndef __NR_ioctl
#define __NR_ioctl 16
#endif
#ifndef __NR_mmap
#define __NR_mmap 9
#endif
#ifndef __NR_socket
#define __NR_socket 41
#endif
#ifndef __NR_setsockopt
#define __NR_setsockopt 54
#endif

#define _GNU_SOURCE

#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

long r[13];
void loop()
{
  memset(r, -1, sizeof(r));
  r[0] = syscall(__NR_mmap, 0x20000000ul, 0x93b000ul, 0x3ul, 0x32ul,
                 0xfffffffffffffffful, 0x0ul);
  r[1] = syscall(__NR_socket, 0x2ul, 0x1ul, 0x0ul);
  memcpy((void*)0x208e8000, "\x45", 1);
  r[3] =
      syscall(__NR_setsockopt, r[1], 0x0ul, 0x0ul, 0x208e8000ul, 0x1ul);
  memcpy((void*)0x2012cfd8,
         "\x73\x69\x74\x30\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
         "\x00",
         16);
  *(uint64_t*)0x2012cfe8 = (uint64_t)0x208e7fe0;
  memcpy((void*)0x208e7fe0,
         "\x01\x00\x00\x00\x09\x00\x02\x00\x00\x03\x06\x00\x00\x00\xeb"
         "\x00\xec\xff\x00\x00\x00\x00\x03\x00\x04\x49\xfa\xf5\x02\x00"
         "\x7e\x23",
         32);
  r[7] = syscall(__NR_ioctl, r[1], 0x89f1ul, 0x2012cfd8ul);
  r[8] = syscall(__NR_socket, 0x2ul, 0x1ul, 0x0ul);
  memcpy((void*)0x20781000,
         "\x01\x00\x01\xe9\x00\x00\x00\x00\x04\x00\xff\xfe\x00\x00\x00"
         "\x02",
         16);
  *(uint64_t*)0x20781010 = (uint64_t)0x208e7fe0;
  memcpy((void*)0x208e7fe0,
         "\x00\x00\x00\x00\x00\x00\x02\x00\x00\x03\xff\xff\xff\x15\xe3"
         "\x00\x19\x00\x00\x00\x00\x00\x03\x00\x04\x49\xfa\xf5\x23\x8f"
         "\x7e\x23",
         32);
  r[12] = syscall(__NR_ioctl, r[8], 0x89f4ul, 0x20781000ul);
}

int main()
{
  loop();
  return 0;
}
