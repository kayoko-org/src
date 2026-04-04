--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

test("env: export variable", function()
    local f = io.popen("env TESTVAR=123 env | grep TESTVAR")
    local res = f:read("*a")
    assert(res:match("TESTVAR=123"), "env failed to set variable")
end)
