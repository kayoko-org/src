#!/usr/bin/env lua
-- cp <src> <dest>

local s, d = arg[1], arg[2]
if not s or not d then return end

local f_src = io.open(s, "rb")
local f_dest = io.open(d, "wb")

if f_src and f_dest then
    f_dest:write(f_src:read("*all"))
    f_src:close()
    f_dest:close()
else
    io.stderr:write("cp: Failed to open files\n")
end
