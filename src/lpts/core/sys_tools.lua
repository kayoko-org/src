test("uname: basic output", function()
    local f = io.popen("uname -s")
    local res = f:read("*a"):trim()
    f:close()
    -- Should be 'Kayoko' or 'NetBSD' depending on your sysname refinement
    assert(#res > 0, "uname -s returned empty string")
end)

test("sleep: duration check", function()
    local start = os.time()
    local ok = os.execute("sleep 1")
    local elapsed = os.time() - start
    assert(ok, "sleep command failed")
    assert(elapsed >= 1, "sleep did not pause for long enough")
end)

test("kill: signal 0 (existence check)", function()
    -- POSIX: kill -0 PID checks if a process exists without signaling it
    -- Fallback: use a known existing PID like 1
    local ok = os.execute("kill -0 1")
    assert(ok, "kill -0 failed to detect init")
end)
