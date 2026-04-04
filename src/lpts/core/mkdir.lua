--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

test("mkdir -p handles existing directories", function()
    -- Clean up first to ensure a fresh state
    os.execute("rm -rf /tmp/lpts_test")
    os.execute("mkdir -p /tmp/lpts_test/a/b")
    
    -- This second call is what tests the EEXIST/S_ISDIR logic
    local ok = os.execute("mkdir -p /tmp/lpts_test/a/b")
    assert(ok, "mkdir -p should not error if directory exists")
end)

test("mkdir -p with symlink in path", function()
    -- Clean up first to prevent "ln: File exists"
    os.execute("rm -rf /tmp/target /tmp/link")
    
    os.execute("mkdir -p /tmp/target")
    os.execute("ln -s /tmp/target /tmp/link")
    
    -- Test if mkdir -p correctly follows the 'link' component
    local ok = os.execute("mkdir -p /tmp/link/newdir")
    assert(ok, "mkdir -p should follow symlinks in the path")
    
    -- Final cleanup
    os.execute("rm -rf /tmp/target /tmp/link")
end)
