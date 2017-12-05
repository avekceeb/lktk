
local sysnr = require("syscalls")

local buf = "12345678"
local file_size = 1024 
local file_name = "XXX"
--local file_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
local trunc_len = 256

-- create file
--[[
local fd = syscall(sysnr.open,
    file_name, (O_RDWR | O_CREAT), file_mode)
assert_gt(fd, 0, "file open")
]]
-- write 1k
local c_total = 0
local c = 0
while c_total < file_size do
    c = syscall(sysnr.write, fd, buf, #buf)
    --assert_ge(c, 0, "write to file")
    c_total = c_total + c
end

assert_eq(syscall(sysnr.close, fd), 0, "close file")

assert_eq(syscall(sysnr.truncate, file_name, trunc_len), 0, "truncate")

--[[
local struct_stat = put_stat()
syscall(sysnr.stat, file_name, struct_stat)
local stat = get_stat(struct_stat)
print(stat.st_size)
]]

require "utils"
local ss = {__type=stat}
syscall(sysnr.stat, file_name, ss)
print("name = "..(ss.__type))
print(table.tostring(ss))
assert_eq(ss.st_size, trunc_len, "truncated successfully, size is OK")

-- ??? syscall(nr, {__NAME=45, x=234})
