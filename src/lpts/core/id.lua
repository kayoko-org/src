test("id: default output format", function()
    local f = io.popen("id")
    local res = f:read("*a")
    f:close()
    -- POSIX format: uid=number(name) gid=number(name) groups=...
    assert(res:match("uid=%d+%(%w+%)"), "id default output format is non-compliant")
end)

test("id: effective vs real uid", function()
    local f_u = io.popen("id -u")
    local u = f_u:read("*a"):trim()
    f_u:close()
    
    local f_ur = io.popen("id -ur")
    local ur = f_ur:read("*a"):trim()
    f_ur:close()
    
    assert(u == ur, "Real and Effective UID should match in standard Kayoko env")
end)

test("id: group list (-G)", function()
    local f = io.popen("id -G")
    local res = f:read("*a")
    f:close()
    -- Should be a space-separated list of numbers
    assert(res:match("^%d+"), "id -G should return space-separated numeric groups")
end)
