#!/usr/bin/env lua

--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

local files = {...}
if #files == 0 then files = {"-"} end

for _, f in ipairs(files) do
    local handle = (f == "-") and io.stdin or io.open(f, "r")
    if not handle then
        io.stderr:write("cat: " .. f .. ": No such file\n")
    else
        for line in handle:lines() do
            print(line)
        end
        if f ~= "-" then handle:close() end
    end
end
