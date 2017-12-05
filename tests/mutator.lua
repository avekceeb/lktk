
local _ = {}

--[[
    TYPES OF ARGS
    -------------
    combination of flags
    in/out pointer to struct
    string:path
    string:arbitrary
    file descr
    pid
    address
    byte count
--]]

--[[ lua types (as returned by type(x):
    "nil", "number", "string", "boolean",
    "table", "function", "thread", "userdata"
]]

-- cache combinations of 1..6 flags
local FlagCombinations6 = {
	[1] = {{},{1}},
	[2] = {{},{1},{2},{1,2}},
	[3] = {{},{1},{2},{3},{1,2},{1,3},{2,3},{1,2,3}},
	[4] = {{},{1},{2},{3},{4},{1,2},{1,3},{1,4},{2,3},{2,4},{3,4},{1,2,3},{1,2,4},{1,3,4},{2,3,4},{1,2,3,4}},
	[5] = {{},{1},{2},{3},{4},{5},{1,2},{1,3},{1,4},{1,5},{2,3},{2,4},{2,5},{3,4},{3,5},{4,5},{1,2,3},{1,2,4},{1,2,5},{1,3,4},{1,3,5},{1,4,5},{2,3,4},{2,3,5},{2,4,5},{3,4,5},{1,2,3,4},{1,2,3,5},{1,2,4,5},{1,3,4,5},{2,3,4,5},{1,2,3,4,5}},
	[6] = {{},{1},{2},{3},{4},{5},{6},{1,2},{1,3},{1,4},{1,5},{1,6},{2,3},{2,4},{2,5},{2,6},{3,4},{3,5},{3,6},{4,5},{4,6},{5,6},{1,2,3},{1,2,4},{1,2,5},{1,2,6},{1,3,4},{1,3,5},{1,3,6},{1,4,5},{1,4,6},{1,5,6},{2,3,4},{2,3,5},{2,3,6},{2,4,5},{2,4,6},{2,5,6},{3,4,5},{3,4,6},{3,5,6},{4,5,6},{1,2,3,4},{1,2,3,5},{1,2,3,6},{1,2,4,5},{1,2,4,6},{1,2,5,6},{1,3,4,5},{1,3,4,6},{1,3,5,6},{1,4,5,6},{2,3,4,5},{2,3,4,6},{2,3,5,6},{2,4,5,6},{3,4,5,6},{1,2,3,4,5},{1,2,3,4,6},{1,2,3,5,6},{1,2,4,5,6},{1,3,4,5,6},{2,3,4,5,6},{1,2,3,4,5,6}}
}

----------------------------------------------------

_.TypeChoice = {
    --set = {}
}

function _.TypeChoice:new(values)
    -- TODO: see same for Flags
    tbl = { set = {} }
    for i,v in ipairs(values) do
        tbl.set[i] = v
    end
    setmetatable(tbl, self)
    self.__index = self
    return tbl
end

function _.TypeChoice:random()
    local n = #(self.set)
    if n < 1 then return nil end
    return self.set[math.random(1,n)]
end

-- function :for_each

----------------------------------------------------

_.TypeInteger = {
    min = math.mininteger,
    max = math.maxinteger
}

function _.TypeInteger:new(tbl)
    tbl = tbl or {}
    setmetatable(tbl, self)
    self.__index = self
    return tbl 
end

function _.TypeInteger:random()
    return math.random(self.min, self.max)
end

----------------------------------------------------

_.TypeFlags = {
    --set = {}
}

--[[
function _.TypeFlags:min() return 0 end

function _.TypeFlags:max()
    local r = 0
    for i,v in ipairs(self.set) do
        r = r | v --TODO: how to bit.bor(all the set) at once?)
    end
    return r
end
]]

function _.TypeFlags:random()
    local r = 0
	local n = #(self.set)
	if n > 6 then return 0 end -- TODO: calculate
	local s = FlagCombinations6[n]
    if 0 == #s then return 0 end
	local rnd = math.random(1,#s)
    if 0 == #(s[rnd]) then return 0 end
    for i,v in ipairs(s[rnd]) do
        r = r | (self.set[v] or 0)
    end
    return r
end

function _.TypeFlags:for_each(f)
	if #(self.set) > 6 then return 0 end
	local s = FlagCombinations6[#(self.set)]
    for i,v in ipairs(s) do
       f(v) 
    end
end

function _.TypeFlags:new(vals)
    -- TODO: is it effective ???
    tbl = {set = {}}
    for i,v in ipairs(vals) do
        tbl.set[i] = v
    end  
    setmetatable(tbl, self)
    self.__index = self
    return tbl
end

----------------------------------------------------

-- TODO: fill real protos
local syscall_prototypes = {
    [1] = {
        _.TypeInteger:new{min=-10, max=-1},
        _.TypeChoice:new{"aaa", "bbb", "ccc"},
        _.TypeFlags:new{2, 16, 4}
    }
}

-- Overload syscall with random args
-- while module is been loaded
local globalsyscall = syscall
syscall = function(n, ...)
    n = n or "???"
    local x = "("
    local args = table.pack(...)
    for i = 1,args.n do
        if "userdata" == type(args[i]) then
            x = x.."{obj}"
        else
            x = x..args[i]
        end
        if i ~= args.n then x = x..", " end
    end
    x = x..")"
    print("Overloading syscall #"..n..x)
    --TODO: should be '[n]'
    local proto = syscall_prototypes[1]
    local l = ""
    for x = 1,#proto do
        l = l.."\t"..proto[x]:random()
    end
    print(l)
    -- TODO: retun global_syscall(.. proto args ..)
    return 0
end

return _
