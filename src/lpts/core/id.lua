-- 1. Default output format
test("id: default output format", function()
    local f = io.popen("id")
    local res = f:read("*a")
    f:close()

    -- POSIX format: uid=number(name) gid=number(name) groups=...
    local pattern = "uid=%d+%(%w[%w%-_%.]*%)"
    if not res:match(pattern) then
        -- If it's missing the name mapping but has the number, it's partial
        if res:match("uid=%d+") then
            partial("Output missing name mapping (e.g., '(root)')")
        else
            error("id default output format is completely non-compliant")
        end
    end
end)

-- 2. Effective vs Real UID
test("id: effective vs real uid", function()
    local f_u = io.popen("id -u")
    local u = f_u:read("*a"):trim()
    f_u:close()

    local f_ur = io.popen("id -ur")
    local ur = f_ur:read("*a"):trim()
    f_ur:close()

    assert(u:match("^%d+$"), "id -u did not return a numeric UID")
    assert(u == ur, "Real and Effective UID should match in standard Kayoko env")
end)

-- 3. Group list (-G) and Names (-Gn)
test("id: group list and names", function()
    local f_g = io.popen("id -G")
    local res_g = f_g:read("*a"):trim()
    f_g:close()
    assert(res_g:match("^%d+"), "id -G should return space-separated numeric groups")

    -- Extension check: -n (name instead of number)
    local f_gn = io.popen("id -Gn 2>/dev/null")
    local res_gn = f_gn:read("*a"):trim()
    f_gn:close()

    if res_gn ~= "" and not res_gn:match("^%d+") then
        extended("Flag -n (names) supported")
    end
end)

-- 4. User lookup (id [user])
test("id: user lookup", function()
    local f = io.popen("id root 2>/dev/null")
    local res = f:read("*a")
    f:close()

    if not res or res == "" then
        error("id failed to look up 'root' user")
    end
    assert(res:match("uid=0"), "id root should show uid=0")
end)

-- 5. Security Context (-Z)
test("id: security context (-Z) (extended)", function()
    local f = io.popen("id -Z 2>/dev/null")
    local res = f:read("*a"):trim()
    f:close()

    if res ~= "" then
        extended("Security context (" .. res .. ") supported via -Z")
    end
end)

-- 6. Process-only flags (-g, -u)
test("id: single category flags", function()
    local f = io.popen("id -g")
    local res = f:read("*a"):trim()
    f:close()

    -- Must be numeric by default
    if not res:match("^%d+$") then
        if res == "" then error("id -g returned no output") end
        partial("id -g returned non-numeric output without -n")
    end
end)
