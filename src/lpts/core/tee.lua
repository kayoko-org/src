--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

test("tee: duplicate output", function()
    os.execute("echo 'double' | tee _tee_1 _tee_2 > /dev/null")
    local f1 = io.open("_tee_1"):read("*a"):trim()
    local f2 = io.open("_tee_2"):read("*a"):trim()
    assert(f1 == "double" and f2 == "double", "tee failed to duplicate output")
    os.execute("rm _tee_1 _tee_2")
end)
