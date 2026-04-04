--[[
Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.
--]]

test("pax: basic archive and list", function()
    os.execute("echo 'test' > lpts_pax_file")
    os.execute("pax -wf archive.pax lpts_pax_file")
    
    local handle = io.popen("pax -f archive.pax")
    local list = handle:read("*a"):trim()
    handle:close()
    
    assert(list == "lpts_pax_file", "pax failed to list archived file")
    os.execute("rm lpts_pax_file archive.pax")
end)
