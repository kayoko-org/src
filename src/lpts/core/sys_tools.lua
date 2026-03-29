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

test("sleep: standard integer duration", function()
    -- POSIX strictly requires supporting integer seconds
    local start = os.time()
    local ok = os.execute("sleep 1")
    local elapsed = os.time() - start
    
    assert(ok, "Standard sleep 1 failed to execute")
    assert(elapsed >= 1, "Standard sleep did not pause for at least 1 second")
    -- If we reach here, lpts.lua will automatically print [PASS]
end)

test("sleep: decimal duration support (extended)", function()
    -- This is a non-POSIX extension (common in GNU/BSD)
    -- We use a small decimal to probe for support
    local start = os.clock()
    local status = os.execute("sleep 0.2 2>/dev/null")
    local elapsed = os.clock() - start

    -- Check if it exited 0 AND actually paused for a short duration
    -- (We use 0.1 as a threshold to account for process overhead)
    if (status == true or status == 0) and elapsed >= 0.1 then
        notice("Decimal durations supported (e.g., sleep 0.2)")
    else
        -- If it fails, we do nothing. 
        -- This test will simply show as [PASS] without the NOTE.
    end
end)

test("kill: signal 0 (existence check)", function()
    -- POSIX: kill -0 PID checks if a process exists without signaling it
    -- Fallback: use a known existing PID like 1
    local ok = os.execute("kill -0 1")
    assert(ok, "kill -0 failed to detect init")
end)
