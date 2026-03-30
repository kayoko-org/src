#!/usr/bin/env lua
local pattern = arg[1]
local file = arg[2]

if not pattern then return end

local handle = file and io.open(file, "r") or io.stdin
for line in handle:lines() do
    if line:find(pattern) then
        print(line)
    end
end
