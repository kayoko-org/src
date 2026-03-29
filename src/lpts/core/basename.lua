test("basename: trailing slashes", function()
    -- POSIX: "trailing <slash> characters shall be deleted"
    local handle = io.popen("basename /usr/bin/")
    local res = handle:read("*a"):trim()
    handle:close()
    assert(res == "bin", "basename /usr/bin/ should be 'bin', got: " .. res)
end)

test("basename: empty string", function()
    -- POSIX: "If string is empty, it is unspecified whether '.' or empty"
    -- Kayoko likely chooses '.' for consistency
    local handle = io.popen("basename ''")
    local res = handle:read("*a"):trim()
    handle:close()
    assert(res == ".", "basename of empty string should be '.'")
end)
