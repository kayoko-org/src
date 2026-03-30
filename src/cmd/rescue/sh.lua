#!/usr/bin/env lua

-- luash: The No-Libc / No-Exec Engine
local prompt = "# "

-- We "BE the SH" by becoming the loader
local function run_tool(name, args)
    local tool_path = name .. ".lua"
    
    -- Load the file as a function
    local func, err = loadfile(tool_path)
    if not func then
        print("sh: tool not found: " .. name)
        return
    end

    -- Inject arguments into the tool's 'arg' table
    -- This mimics how Lua normally handles command line args
    local old_arg = arg
    arg = args
    
    local ok, msg = pcall(func)
    if not ok then
        print("sh: error in " .. name .. ": " .. tostring(msg))
    end
    
    arg = old_arg -- Restore original args
end

print("\n")
while true do
    io.write(prompt)
    io.flush()
    local input = io.read()
    if not input or input == "exit" then break end

    -- Simple parser: first word is tool, rest are args
    local parts = {}
    for part in input:gmatch("%S+") do
        table.insert(parts, part)
    end
    
    if #parts > 0 then
        local cmd = table.remove(parts, 1)
        run_tool(cmd, parts)
    end
end
