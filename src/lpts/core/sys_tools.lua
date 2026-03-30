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
        local pf = io.popen("uname " .. flag .. " 2>/dev/null")
        local output = pf:read("*a"):trim()
        local ok = pf:close()

        if ok and #output > 0 then
            table.insert(found_extras, flag .. ": " .. output)
        end
    end

    if #found_extras > 0 then
        -- Keep as NOTICE: This is environmental metadata, not a tool "feature"
        notice("System Metadata: " .. table.concat(found_extras, " | "))
    end
end)

test("sleep: standard integer duration", function()
    local start = os.time()
    local ok = os.execute("sleep 1")
    local elapsed = os.time() - start

    assert(ok, "Standard sleep 1 failed to execute")
    assert(elapsed >= 1, "Standard sleep did not pause for at least 1 second")
end)

test("sleep: decimal duration support (extended)", function()
    -- Non-POSIX extension
    local start = os.clock()
    local status = os.execute("sleep 0.2 2>/dev/null")
    local elapsed = os.clock() - start

    if (status == true or status == 0) and elapsed >= 0.1 then
        -- Use EXTENDED: This is a functional addition to the tool
        extended("Decimal durations supported (e.g., sleep 0.2)")
    end
end)

test("kill: signal 0 (existence check)", function()
    -- Standard POSIX check
    local ok = os.execute("kill -0 1")
    assert(ok, "kill -0 failed to detect init")
end)
