--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

test("chmod: octal mode", function()
    os.execute("touch _chmod_test")
    os.execute("chmod 644 _chmod_test")
    local f = io.popen("ls -l _chmod_test")
    local res = f:read("*a")
    f:close()
    assert(res:match("^%-rw%-r%-%-r%-%-"), "chmod octal failed: " .. res)
    os.execute("rm _chmod_test")
end)

test("chmod: symbolic addition", function()
    os.execute("touch _chmod_sym")
    os.execute("chmod 600 _chmod_sym")
    os.execute("chmod u+x _chmod_sym")
    local f = io.popen("ls -l _chmod_sym")
    local res = f:read("*a")
    f:close()
    assert(res:match("^%-rwx"), "chmod symbolic u+x failed")
    os.execute("rm _chmod_sym")
end)

test("chmod: comma separated symbols", function()
    os.execute("touch _chmod_multi")
    os.execute("chmod 644 _chmod_multi")
    -- POSIX: multiple clauses separated by commas
    os.execute("chmod g+w,o-r _chmod_multi")
    local f = io.popen("ls -l _chmod_multi")
    local res = f:read("*a")
    f:close()
    assert(res:match("^%-rw%-rw%-%-%-%-"), "chmod multi-symbol failed")
    os.execute("rm _chmod_multi")
end)
