m = require("mutator")
require("utils")

local f = m.TypeFlags:new{0x01, 0x10, 0x0100}
local valid_f = {
    [0] = true,
    [1] = true,
    [16] = true,
    [17] = true,
    [256] = true,
    [257] = true,
    [272] = true,
    [273] = true
}
local v
for j = 1,10 do
    v = f:random()
	assert_true(valid_f[v] ~= nil, v.." is expected")
end

local c = m.TypeChoice:new{"XX", "YY", "ZZ"}
local valid_c = {XX = true, YY = true, ZZ = true}
for j = 1,10 do
    v = c:random()
	assert_true(valid_c[v] ~= nil, v.." is expected")
end

--[[
function F(t) print(table.tostring(t)) end
f:for_each(F)
]]

local prototype_one = {
    m.TypeInteger:new{min=-10, max=-1},
    m.TypeChoice:new{"aaa", "bbb", "ccc"},
    m.TypeFlags:new{2, 16, 4}
}

for i = 1,10 do
    local l = ""
    for x = 1, #prototype_one do
        l = l.."\t"..prototype_one[x]:random()
    end
    print(l)
end

--[[
local z = m.TypeFlags:new{2, 16, 4}
print(table.tostring(z.set))
]]

local i = m.TypeInteger:new{min = -10, max = 10}
print(string.format("%d - %d", i.min, i.max))
local ii = m.TypeInteger:new()
print(string.format("%d - %d", ii.min, ii.max))

