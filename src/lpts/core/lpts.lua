--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

-- ==========================================================
-- lpts.lua - Kayoko Test Runner (Partial & Notice Support)
-- ==========================================================

function string:trim()
   return self:match("^%s*(.-)%s*$")
end

local lpts = {
    total = 0, passed = 0, failed = 0, partial = 0, notices = 0, extended = 0,
    details = {},
    timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ")
}

local colors = {
    reset   = "\27[0m",
    green   = "\27[32m",
    red     = "\27[31m",
    yellow  = "\27[33m",
    blue    = "\27[34m", 
    magenta = "\27[35m", -- Color for Extended
    cyan    = "\27[36m"
}

local function save_json(path, data)
    local f = io.open(path, "w")
    if not f then return end
    local function encode(v)
        if type(v) == "string"  then return string.format("%q", v) end
        if type(v) == "number" or type(v) == "boolean" then return tostring(v) end
        if type(v) == "table" then
            local is_array = #v > 0
            local parts = {}
            if is_array then
                for _, val in ipairs(v) do table.insert(parts, encode(val)) end
                return "[" .. table.concat(parts, ",") .. "]"
            else
                for k, val in pairs(v) do 
                    table.insert(parts, string.format("%q:%s", k, encode(val))) 
                end
                return "{" .. table.concat(parts, ",") .. "}"
            end
        end
        return "null"
    end
    f:write(encode(data))
    f:close()
end

local current_file = ""
local flags = { partial = false, notice = false, extended = false }
local msgs  = { partial = "", notice = "", extended = "" }

-- Helper functions for tests
_G.partial  = function(msg) flags.partial = true; msgs.partial = msg or "Partial success" end
_G.notice   = function(msg) flags.notice = true;  msgs.notice = msg or "Notice info" end
_G.extended = function(msg) flags.extended = true; msgs.extended = msg or "Extended feature" end

_G.test = function(description, fn)
    lpts.total = lpts.total + 1
    flags.partial, flags.notice, flags.extended = false, false, false
    msgs.partial, msgs.notice, msgs.extended = "", "", ""

    local ok, err = pcall(fn)

    local status = "PASS"
    if not ok then status = "FAIL"
    elseif flags.partial then status = "PARTIAL"
    elseif flags.extended then status = "EXTENDED"
    elseif flags.notice then status = "NOTICE" end

    table.insert(lpts.details, {
        file = current_file,
        test = description,
        result = status,
        note = flags.partial and msgs.partial or (flags.extended and msgs.extended or (flags.notice and msgs.notice or nil)),
        error = (not ok) and err or nil
    })

    if not ok then
        lpts.failed = lpts.failed + 1
        print(string.format("  [%sFAIL%s] %s: %s", colors.red, colors.reset, description, err))
    elseif flags.partial then
        lpts.partial = lpts.partial + 1
        print(string.format("  [%sPART%s] %s (%s)", colors.yellow, colors.reset, description, msgs.partial))
    elseif flags.extended then
        lpts.extended = lpts.extended + 1
        print(string.format("  [%sEXTD%s] %s (%s)", colors.magenta, colors.reset, description, msgs.extended))
    elseif flags.notice then
        lpts.notices = lpts.notices + 1
        print(string.format("  [%sNOTE%s] %s (%s)", colors.blue, colors.reset, description, msgs.notice))
    else
        lpts.passed = lpts.passed + 1
        print(string.format("  [%sPASS%s] %s", colors.green, colors.reset, description))
    end
end

local p = io.popen("ls *.lua")
for file in p:lines() do
    if file ~= "lpts.lua" then
        current_file = file
        print(string.format("\n--> %s", file))
        dofile(file)
    end
end
p:close()

-- Summary
print("\n" .. string.rep("=", 30))
print(string.format("Total:    %d", lpts.total))
print(string.format("Passed:   %s%d%s", colors.green, lpts.passed, colors.reset))
print(string.format("Extended: %s%d%s", colors.magenta, lpts.extended, colors.reset))
print(string.format("Partial:  %s%d%s", colors.yellow, lpts.partial, colors.reset))
print(string.format("Notices:  %s%d%s", colors.blue, lpts.notices, colors.reset))
print(string.format("Failed:   %s%d%s", colors.red, lpts.failed, colors.reset))
print(string.rep("=", 30))

save_json("lpts.json", lpts)
os.exit(lpts.failed == 0 and 0 or 1)
