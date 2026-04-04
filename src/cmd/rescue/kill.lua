#!/usr/bin/env lua

--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]


local sig = arg[1]
local pid = arg[2]

if not sig or not pid then
    print("Usage: lua kill.lua <signal_num> <pid>")
    os.exit(1)
end

-- Use os.execute to hit the system's kill command
local ok = os.execute("kill -" .. sig .. " " .. pid .. " 2>/dev/null")
if not ok then
    print("Failed to send signal " .. sig .. " to " .. pid)
end
