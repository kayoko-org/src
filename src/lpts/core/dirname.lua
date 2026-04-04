--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

local cases = {
    -- [Input]              = [Expected Output]
    ["/usr/bin/lua"]        = "/usr/bin",
    ["/usr/bin/"]           = "/usr",
    ["/usr"]                = "/",
    ["/"]                   = "/",
    ["."]                   = ".",
    [".."]                  = ".",
    ["lua"]                 = ".",
    ["/lib//modules"]       = "/lib",
    ["//standalone"]        = "/", -- Or "//", depending on OS implementation
    [""]                    = ".",
}

for path, expected in pairs(cases) do
    test("dirname: " .. (path == "" and "empty string" or path), function()
        local f = io.popen(string.format("dirname %q 2>/dev/null", path))
        local result = f:read("*a"):trim()
        f:close()
        
        assert(result == expected, 
            string.format("\n  Path: %s\n  Expected: %s\n  Got: %s", path, expected, result))
    end)
end
