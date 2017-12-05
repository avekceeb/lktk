local nr = require "syscalls"

addr = "\xcf\x0b\x0b\x99\x22\x33\x96\xdf\xbd\x2e\x29\x1b\x4d\xc0\x2a\xee\x03"

for i = 1,10 do
    local fd = syscall(nr.open, "/dev/net/tun", 0, 0)
    syscall(nr.ioctl, fd, 0x400454ca, addr);
end
