#!/usr/bin/env lua

--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

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
