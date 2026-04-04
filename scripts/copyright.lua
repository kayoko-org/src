#!/usr/bin/env lua
-- 1. Define the raw legal text

local text = {

    cddl = [[Kayoko - Source Code

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Common Development and Distribution License (CDDL).

See /usr/src/COPYING for details.]],


    pdl = [[Kayoko - Documentation

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Public Documentation License (PDL).

See /usr/src/DOCCOPYING for details.]]

}


-- 2. Define how to wrap text based on comment style

local function wrap(style, content)

    if style == "c" then 

        return "/*\n * " .. content:gsub("\n", "\n * ") .. "\n */\n"

    elseif style == "lua" then

        return "--[[\n" .. content .. "\n--]]\n"

    elseif style == "xml" then
	   return "<!--\n" .. content .. "\n-->\n"
    elseif style == "hash" then

        return "#\n# " .. content:gsub("\n", "\n# ") .. "\n#\n"

    end

    return content

end


-- 3. Extension Map (Added .sh)

local extension_map = {

    -- Code (CDDL)

    lua  = {"cddl", "lua"},

    c    = {"cddl", "c"},

    h    = {"cddl", "c"},

    sh   = {"cddl", "hash"}, -- Shell scripts use hash comments

    -- Docs (PDL)

    md   = {"pdl", "xml"},

    xml  = {"pdl", "xml"},

    html = {"pdl", "xml"}

}


-- 4. Logic to prepend header

local function apply_header(filename)

    local ext = filename:match("^.+(%..+)$")

    if not ext then return end

    ext = ext:sub(2)


    local config = extension_map[ext]

    if not config then return end


    local license_type, comment_style = config[1], config[2]

    

    local f = io.open(filename, "r")

    if not f then return end

    local content = f:read("*all")

    f:close()


    -- Avoid double-licensing

    if content:find("Licensed under") then return end


    local header = wrap(comment_style, text[license_type])

    local output = ""


    -- Handle specialized first-line declarations (Shebang or XML)

    local first_line_pattern = "^(#!.-\n)"          -- Shebang

    if ext == "xml" then first_line_pattern = "^(<%?xml.-%?>%s*\n?)" end

    

    local leading_meta = content:match(first_line_pattern)


    if leading_meta then

        -- Insert header AFTER the shebang or xml declaration

        output = leading_meta .. "\n" .. header .. "\n" .. content:sub(#leading_meta + 1)

    else

        -- Standard prepend

        output = header .. "\n" .. content

    end


    f = io.open(filename, "w")

    f:write(output)

    f:close()

    print("✓ " .. license_type:upper() .. " -> " .. filename)

end


-- 5. Recursive Walk (Unix/Linux/macOS)

local function walk(path)

    local popen = io.popen

    local pfile = popen('find "' .. path .. '" -type f')

    

    for filename in pfile:lines() do

        -- Skip hidden files/folders (like .git)

        if not filename:find("/%.") then

            apply_header(filename)

        end

    end

    pfile:close()

end


-- Main

local target_dir = arg[1] or "."

print("Applying CDDL/PDL headers recursively to: " .. target_dir)

walk(target_dir)
