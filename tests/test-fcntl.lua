local nr = require "syscalls"
local bit = bit32

local fd = {}
local file_perm = bit.bor(S_IRWXU, S_IRGRP)

for i = 1,8 do
    fd[i] = syscall(nr.open,
        string.format("./fcntl%d", i),
        bit.bor(O_WRONLY, O_CREAT),
        file_perm)
    assert_gt(fd[i], 0, "open file")
end

for i = 3,6 do
    syscall(nr.close, fd[i])
end

assert_gt(syscall(nr.fcntl, fd[1], F_DUPFD, 1), 0)
assert_gt(syscall(nr.fcntl, fd[1], F_DUPFD, fd[2]), fd[3])

--[[
for x,y in pairs(fd) do
    print(x.." = "..y)
end
]]
