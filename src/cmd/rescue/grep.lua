#!/usr/bin/env lua

--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

local pattern = arg[1]
local file = arg[2]

if not pattern then return end

local handle = file and io.open(file, "r") or io.stdin
for line in handle:lines() do
    if line:find(pattern) then
        print(line)
    end
end
