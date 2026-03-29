-- ==========================================================
-- lpts.lua - Kayoko Test Runner
-- Scans ./*.lua (excluding self) and executes POSIX tests.
-- ==========================================================

function string:trim()
   return self:match("^%s*(.-)%s*$")
end

local lpts = { total = 0, passed = 0, failed = 0 }
local colors = {
    reset = "\27[0m",
    green = "\27[32m",
    red   = "\27[31m",
    cyan  = "\27[36m"
}

-- Global test function for use in the scanned files
_G.test = function(description, fn)
    lpts.total = lpts.total + 1
    local ok, err = pcall(fn)
    if ok then
        lpts.passed = lpts.passed + 1
        print(string.format("  [%sPASS%s] %s", colors.green, colors.reset, description))
    else
        lpts.failed = lpts.failed + 1
        print(string.format("  [%sFAIL%s] %s", colors.red, colors.reset, description))
        print(string.format("         %s", err))
    end
end

print(string.format("%s==> Kayoko POSIX Test Suite <==%s", colors.cyan, colors.reset))

-- Use ls to find all .lua files, excluding the runner itself
local p = io.popen("ls *.lua")
for file in p:lines() do
    if file ~= "lpts.lua" then
        print(string.format("\nFILE: %s", file))
        dofile(file)
    end
end
p:close()

-- Summary
local status_color = (lpts.failed == 0) and colors.green or colors.red
print(string.format("Results: %s%d/%d passed%s", status_color, lpts.passed, lpts.total, colors.reset))

os.exit(lpts.failed == 0 and 0 or 1)
