-- lpts/core/true_false.lua

test("true returns 0", function()
    -- os.execute returns: ok (boolean), type (string), code (number)
    local ok, _, exit_code = os.execute("true")
    assert(ok == true, "true must return success (boolean true)")
    assert(exit_code == 0, "true must return exit status 0")
end)

test("false returns non-zero", function()
    local ok, _, exit_code = os.execute("false")
    -- ok will be nil (falsy) in Lua 5.2+, or non-zero (truthy!) in 5.1
    -- This is why testing POSIX is tricky even in the test suite!
    assert(not ok or ok ~= 0, "false must return a non-zero/failure status")
end)
