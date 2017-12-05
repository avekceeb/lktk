local struct = {
	__NAME = -1
}

function struct:new(vals)
    tbl = vals or {}
    setmetatable(tbl, self)
    self.__index = self
    return tbl
end

return struct