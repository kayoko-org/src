#!/usr/bin/env lua
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
