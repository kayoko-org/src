#!/usr/bin/env lua

--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]


local function get_mounts()
    -- On NetBSD, 'mount' output is usually: /dev/xbd0a on / type ffs (local)
    local pipe = io.popen("mount")
    print(string.format("%-20s %-10s %s", "Device", "Type", "Mount Point"))
    print(string.rep("-", 45))
    
    for line in pipe:lines() do
        local dev, mp, mtype = line:match("^(%S+) on (%S+) type (%S+)")
        if dev then
            print(string.format("%-20s %-10s %s", dev, mtype, mp))
        end
    end
    pipe:close()
end

get_mounts()
