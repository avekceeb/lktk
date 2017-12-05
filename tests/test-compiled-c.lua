local ok, ret = os.execute('gcc warn-4.12.c -o kernwarn')
assert_true(ok, 'compilation')
ok, ret = os.execute('./kernwarn')
assert_true(ok, 'compiled executable')
--[[
local handle = io.popen(command)
local result = handle:read("*a")
handle:close()
]]
