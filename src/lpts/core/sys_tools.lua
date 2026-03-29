test("uname: basic output & extended flags", function()
    -- 1. Standard POSIX: Get the system name
    local f = io.popen("uname -s")
    local res = f:read("*a"):trim()
    f:close()
    assert(#res > 0, "uname -s returned empty string")

    -- 2. Probe for non-standard Extended flags
    local extra_flags = { "-o", "-i", "-p", "-v" }
    local found_extras = {}

    for _, flag in ipairs(extra_flags) do
        -- Use 2>/dev/null to silencing "illegal option" errors from the shell
        local pf = io.popen("uname " .. flag .. " 2>/dev/null")
        local output = pf:read("*a"):trim()
        local ok = pf:close()

        if ok and #output > 0 then
            table.insert(found_extras, flag .. ": " .. output)
        end
    end

    if #found_extras > 0 then
        notice("Extended metadata found: " .. table.concat(found_extras, " | "))
    end
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
