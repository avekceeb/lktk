
local sys = require "syscalls"
local bit = bit32

function print_table(tbl)
    for i,v in pairs(tbl) do
        print(i.." -> "..v)
    end
end

print("--- syscalls (void) ----")

assert_gt(syscall(sys.getpid), 0)
assert_gt(syscall(sys.getppid), 0)
assert_ge(syscall(sys.getuid), 0)
assert_ge(syscall(sys.geteuid), 0)
assert_eq(syscall(1984), -1, "wrong syscall should fail")

print("--- sched_attr tests ---")

local sa = {
	__type = sched_attr;
    sched_policy = 0x01;
    sched_flags = 0x02;
    sched_nice = 0x03;
    --sched_priority = 0x04;
    sched_runtime = 0x05;
    sched_deadline = 0x06;
    sched_period = 0x07;
}

assert_eq(
	syscall(
		sys.sched_getattr, 0, sa, sizeof_sched_attr(), 0),
	0,
	"sched_getattr")
--[[
assert_eq(syscall(sys.sched_getattr,0,sa,0,0), -1, "sched_getattr wrong size")
syscall()
]]

-- TODO: in cycle
syscall(sys.request_key, "dead", "abc", "abc", 0)

print("----- lock test ----")
local pid = syscall(sys.getpid)
local filename = "XXXX."..pid

local fd = syscall(sys.open,
    filename,
    bit.bor(O_CREAT, O_RDWR),
    bit.bor(S_IRWXU, S_IRGRP))
assert_gt(fd, 1, "file created")

local lck = {
    __type = flock;
    l_type = F_WRLCK;     -- Exclusive lock
    l_whence = SEEK_SET;  -- Relative to beginning of file
    l_start = 0;            -- Start from 1st byte
    l_len = 0;              -- Lock whole file
}

local result = syscall(sys.fcntl, fd, F_SETLK, lck)
assert_ge(result, 0, "lock succesfull")

local test_msg = "Hello World!\n" 
assert_eq(
    syscall(sys.write, fd, test_msg, #test_msg),
    #test_msg, "Number of bytes written")

local unlck = { __type = flock;
    l_type=F_UNLCK;
    l_whence=SEEK_SET;
    l_start=0;
    l_len=0;}

assert_ge(
    syscall(sys.fcntl, fd, F_SETLK, unlck),
    0, "lock unset")

syscall(sys.unlink, filename)

