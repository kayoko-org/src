#!/usr/bin/env lua

--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

--  ee (Line Editor)
local file = arg[1]
if not file then print("Usage: ee <file>") return end

print("Editing " .. file .. " (Type . on a new line to save/exit)")
local lines = {}
while true do
    local line = io.read()
    if line == "." then break end
    table.insert(lines, line)
end

local f = io.open(file, "w")
f:write(table.concat(lines, "\n") .. "\n")
f:close()
