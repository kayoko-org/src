#!/usr/bin/env lua

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
