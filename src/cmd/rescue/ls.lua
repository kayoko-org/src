#!/usr/bin/env lua

--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]


local path = "."
local mode = "default"

-- Simple argument parsing
for i = 1, #arg do
    if arg[i] == "-l" then
        mode = "long"
    elseif arg[i]:sub(1,1) ~= "-" then
        path = arg[i]
    end
end

-- 1. Get the file list via a pipe to a simple 'ls -1a'
-- We use -a to ensure we see hidden files, but we'll filter . and ..
local function get_files(p)
    local files = {}
    local pipe = io.popen("ls -1a " .. p .. " 2>/dev/null")
    if not pipe then return nil end
    
    for line in pipe:lines() do
        if line ~= "." and line ~= ".." then
            table.insert(files, line)
        end
    end
    pipe:close()
    return files
end

-- 2. The "JeTools" Long Format: Raw data dump
-- Using NetBSD 'stat' format strings to get the essentials
local function get_stat_raw(filepath)
    -- %z: size, %p: octal perm, %u/%g: IDs, %m: mtime
    local cmd = "stat -f '%p %u %g %z %m' " .. filepath .. " 2>/dev/null"
    local pipe = io.popen(cmd)
    if not pipe then return "?" end
    local res = pipe:read("*all"):gsub("\n", "")
    pipe:close()
    return res == "" and "stat_fail" or res
end

local files = get_files(path)

if not files then
    io.stderr:write("ls: " .. path .. ": No such file or directory\n")
    os.exit(1)
end

-- Lua gives us easy sorting for free
table.sort(files)

for _, name in ipairs(files) do
    if mode == "long" then
        local full_path = (path == "." and name) or (path:sub(-1) == "/" and path .. name) or (path .. "/" .. name)
        local raw = get_stat_raw(full_path)
        -- Output: [Perms] [UID] [GID] [Size] [MTime] [Name]
        print(string.format("%s  %s", raw, name))
    else
        -- Just Enough: 1 column
        print(name)
    end
end
