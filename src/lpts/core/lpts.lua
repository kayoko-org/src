-- ==========================================================
-- lpts.lua - Kayoko Test Runner (with JSON Logging)
-- ==========================================================

function string:trim()
   return self:match("^%s*(.-)%s*$")
end

local lpts = { 
    total = 0, passed = 0, failed = 0, 
    details = {}, 
    timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ") 
}

local colors = {
    reset = "\27[0m",
    green = "\27[32m",
    red   = "\27[31m",
    cyan  = "\27[36m"
}

-- Simple JSON encoder for the results table
local function save_json(path, data)
    local f = io.open(path, "w")
    if not f then return end
    
    local function encode(v)
        if type(v) == "string"  then return string.format("%q", v) end
        if type(v) == "number"  then return tostring(v) end
        if type(v) == "boolean" then return tostring(v) end
        if type(v) == "table" then
            local is_array = #v > 0
            local parts = {}
            if is_array then
                for _, val in ipairs(v) do table.insert(parts, encode(val)) end
                return "[" .. table.concat(parts, ",") .. "]"
            else
                for k, val in pairs(v) do table.insert(parts, string.format("%q:%s", k, encode(val))) end
                return "{" .. table.concat(parts, ",") .. "}"
            end
        end
        return "null"
    end

    f:write(encode(data))
    f:close()
end

local current_file = ""

_G.test = function(description, fn)
    lpts.total = lpts.total + 1
    local ok, err = pcall(fn)
    local status = ok and "PASS" or "FAIL"
    
    -- Store for JSON
    table.insert(lpts.details, {
        file = current_file,
        test = description,
        result = status,
        error = err or nil
    })

    if ok then
        lpts.passed = lpts.passed + 1
        print(string.format("  [%sPASS%s] %s", colors.green, colors.reset, description))
    else
        lpts.failed = lpts.failed + 1
        print(string.format("  [%sFAIL%s] %s", colors.red, colors.reset, description))
    end
end

print(string.format("%s==> Kayoko POSIX Test Suite <==%s", colors.cyan, colors.reset))

local p = io.popen("ls *.lua")
for file in p:lines() do
    if file ~= "lpts.lua" then
        current_file = file
        print(string.format("\nFILE: %s", file))
        dofile(file)
    end
end
p:close()

-- Summary and Export
local status_color = (lpts.failed == 0) and colors.green or colors.red
print(string.format("\nResults: %s%d/%d passed%s", status_color, lpts.passed, lpts.total, colors.reset))

save_json("lpts.json", lpts)
print(string.format("Log written to %slpts.json%s", colors.cyan, colors.reset))

os.exit(lpts.failed == 0 and 0 or 1)
