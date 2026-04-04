--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

test("rm: basic unlink", function()
    os.execute("touch _rm_test")
    os.execute("rm _rm_test")
    local f = io.open("_rm_test", "r")
    assert(f == nil, "rm failed to remove file")
end)

test("rm -r: recursive directory removal", function()
    os.execute("mkdir -p _rm_dir/a/b")
    os.execute("touch _rm_dir/a/b/file")
    local ok = os.execute("rm -r _rm_dir")
    assert(ok, "rm -r failed to remove directory tree")
end)

test("mv: rename and move", function()
    os.execute("echo 'move_me' > _mv_src")
    os.execute("mv _mv_src _mv_dest")
    local f = io.open("_mv_dest", "r")
    local content = f:read("*a"):trim()
    f:close()
    assert(content == "move_me", "mv failed to move content")
    os.execute("rm _mv_dest")
end)

test("cp: simple copy", function()
    os.execute("echo 'copy_me' > _cp_src")
    os.execute("cp _cp_src _cp_dest")
    local f = io.open("_cp_dest", "r")
    assert(f ~= nil, "cp failed to create destination")
    f:close()
    os.execute("rm _cp_src _cp_dest")
end)
