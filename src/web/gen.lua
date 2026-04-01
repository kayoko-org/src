-- Configuration
local TEMPLATE_FILE = "template.html"
local DATA_DIR = "data"
local BUILD_DIR = "build"
local FRAGMENTS = { header = "fragments/header.html", footer = "fragments/footer.html" }

local function read_file(path)
    local f = io.open(path, "r")
    if not f then return "" end
    local content = f:read("*all")
    f:close()
    return content
end

-- 1. Use 'find' to get all .html files in the data directory
-- We use -name to filter and -prune if you wanted to avoid subdirs, 
-- but here we'll keep it simple.
local pages = {}
local command = string.format("find %s -name '*.html' -type f", DATA_DIR)
local p = io.popen(command)

for path in p:lines() do
    -- Extract the filename without the directory and extension
    -- Example: data/my_page.html -> my_page
    local name = path:match("([^/]+)%.html$")
    if name then
        table.insert(pages, { name = name, path = path })
    end
end
p:close()

-- Sort pages alphabetically for the sidebar
table.sort(pages, function(a, b) return a.name < b.name end)

-- 2. Build the Sidebar HTML
local sidebar_html = "<ul>\n"
for _, page in ipairs(pages) do
    sidebar_html = sidebar_html .. string.format('  <li><a href="%s.html">%s</a></li>\n', page.name, page.name)
end
sidebar_html = sidebar_html .. "</ul>"

-- 3. Load Template & Fragments
local template = read_file(TEMPLATE_FILE)
local header = read_file(FRAGMENTS.header)
local footer = read_file(FRAGMENTS.footer)

-- 4. Process Each Page
for _, page in ipairs(pages) do
    local raw_content = read_file(page.path)

    -- Wiki Link Parser: [[PageName]] -> <a href="PageName.html">PageName</a>
    local content = raw_content:gsub("%[%[(.-)%]%]", function(link)
        return string.format('<a href="%s.html">%s</a>', link, link)
    end)

    -- Stitching with basic escaping for '%' in content to avoid gsub errors
    local output = template
    output = output:gsub("{{header}}", function() return header end)
    output = output:gsub("{{footer}}", function() return footer end)
    output = output:gsub("{{sidebar}}", function() return sidebar_html end)
    output = output:gsub("{{content}}", function() return content end)
    output = output:gsub("{{title}}", function() return page.name end)

    local out_file = io.open(BUILD_DIR .. "/" .. page.name .. ".html", "w")
    if out_file then
        out_file:write(output)
        out_file:close()
        print("Stitched: " .. page.name)
    end
end

print("Wiki generation complete.")
