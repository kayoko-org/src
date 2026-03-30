#!/usr/bin/env lua
local files = {...}
if #files == 0 then files = {"-"} end

for _, f in ipairs(files) do
    local handle = (f == "-") and io.stdin or io.open(f, "r")
    if not handle then
        io.stderr:write("cat: " .. f .. ": No such file\n")
    else
        for line in handle:lines() do
            print(line)
        end
        if f ~= "-" then handle:close() end
    end
end
