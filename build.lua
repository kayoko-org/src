#!/usr/bin/env lua

local os = require("os")
local io = require("io")

-- --- 1. UI Configuration & Color Handling ---
local ui = {
    bold  = "\27[1m",
    dim   = "\27[2m",
    red   = "\27[31m",
    reset = "\27[0m"
}

-- Check if output is a TTY (Requires io.popen for portability)
local function is_tty()
    local h = io.popen("test -t 1 && echo yes")
    local res = h:read("*all")
    h:close()
    return res:match("yes") ~= nil
end

local color_mode = "auto" -- default

-- Initial pass for color flags
for i=1, #arg do
    if arg[i] == "--color=none" then color_mode = "none" 
    elseif arg[i] == "--color=always" then color_mode = "always" end
end

-- Strip colors if needed
if color_mode == "none" or (color_mode == "auto" and not is_tty()) then
    for k, _ in pairs(ui) do ui[k] = "" end
end


-- --- 2. Core Configuration ---
local cfg = {
    root     = os.getenv("KAYAKO_ROOT") or (os.getenv("PWD") .. "/root"),
    obj      = os.getenv("OBJ_DIR")     or (os.getenv("PWD") .. "/root/obj"),
    tools    = os.getenv("TOOL_DIR")    or (os.getenv("PWD") .. "/root/tools"),
    src      = os.getenv("NBSD_SRC")    or (os.getenv("PWD") .. "/src/gate"),
    arch     = os.getenv("ARCH")        or "amd64",
    inst     = os.getenv("_INST")       or (os.getenv("PWD") .. "/inst"),
    nprocs   = os.getenv("NPROCS")      or "4",
    kernel   = "KAYAKO",
    without  = {}
}

-- Parse Arguments
for i=1, #arg do
    if arg[i]:match("^%-%-without%-") then
        local target = arg[i]:match("^%-%-without%-(.+)$")
        cfg.without[target] = true
    end
end

cfg.destdir = cfg.obj .. "/destdir." .. cfg.arch
cfg.nbmake  = cfg.tools .. "/bin/nbmake-" .. cfg.arch
cfg.flags   = string.format("DESTDIR=%s UNPRIVILEGED=no MKUNPRIVILEGED=no", cfg.destdir)

-- --- 3. Helper Functions ---

local function format_output(cmd)
    -- Parse CC: cc ... -o <bin> <src>
    local bin, src = cmd:match("cc .*%-o%s+(%S+)%s+(%S+)")
    if bin and src then
        -- Return [CC] src -o bin in DIM
        local short_bin = bin:match("([^/]+)$") or bin
        local short_src = src:match("([^/]+)$") or src
        return string.format("%s[CC] %s -o %s%s", ui.dim, short_src, short_bin, ui.reset)
    elseif cmd:match("make") then
        -- Return [MAKE] in DIM
        return string.format("%s[MAKE] %s%s", ui.dim, cmd, ui.reset)
    else
        -- Regular steps in BOLD WHITE
        return string.format("%s--> %s%s", ui.bold, cmd, ui.reset)
    end
end

local function sh(cmd)
    print(format_output(cmd))
    local success = os.execute(cmd .. " > /dev/null 2>&1")
    if not success then
        print(string.format("\n[!] Command failed: %s", cmd))
        os.exit(1)
    end
end

local function step(msg)
    print(string.format("%s--> %s%s", ui.bold, msg, ui.reset))
end

local function needs_update(source, target)
    local f_t = io.open(target, "r")
    if not f_t then return true end
    f_t:close()
    local get_time = function(f)
        local h = io.popen("stat -f %m " .. f)
        local out = h:read("*all")
        h:close()
        return tonumber(out) or 0
    end
    return get_time(source) > get_time(target)
end

-- --- 4. Task Definitions ---
local tasks = {}

tasks["init"] = function()
    step("Initializing Kayoko environment...")
    local dirs = {"/bin", "/sbin", "/lib", "/etc", "/usr/bin", "/usr/lib"}
    for _, d in ipairs(dirs) do sh("mkdir -p " .. cfg.root .. d) end
    sh("ln -sf ../lib " .. cfg.root .. "/usr/lib")
    sh(string.format("cd %s/etc && %s %s distrib-dirs", cfg.src, cfg.nbmake, cfg.flags))
    sh(string.format("cd %s && %s %s includes", cfg.src, cfg.nbmake, cfg.flags))
end

tasks["branding"] = function()
    step("Applying Kayoko Branding")
    local vers_script = cfg.src .. "/sys/conf/newvers.sh"
    local f = io.open(vers_script, "r")
    if not f then return end
    local content = f:read("*all")
    f:close()
    local new_content, count = content:gsub('ost="NetBSD"', 'ost="Kayoko"')
    if count > 0 then
        local wf = io.open(vers_script, "w")
        wf:write(new_content)
        wf:close()
    end
    sh("touch " .. cfg.src .. "/.kayoko_branded")
end

tasks["tools"] = function()
    if not io.open(cfg.nbmake, "r") then
        step("Building NetBSD Toolchain")
        sh(string.format("cd %s && env -i PATH=/usr/bin:/bin TERM=$TERM HOME=$HOME ./build.sh -m %s -T %s -O %s -u tools",
            cfg.src, cfg.arch, cfg.tools, cfg.obj))
    end
end

tasks["base"] = function()
    step("Building Base System")
    local function build_nbsd_component(path)
        local lflags = string.format("DESTDIR=%s UNPRIVILEGED=no MKUNPRIVILEGED=no", cfg.destdir)
        sh(string.format("cd %s/%s && %s %s obj && %s %s depend && %s %s -j%s all install",
            cfg.src, path, cfg.nbmake, lflags, cfg.nbmake, lflags, cfg.nbmake, lflags, cfg.nprocs))
    end
    build_nbsd_component("lib/csu")
    build_nbsd_component("external/gpl3/gcc/lib/libgcc")
    build_nbsd_component("lib/libc")
    sh(string.format("cp -P %s/lib/libc.s* %s/lib/", cfg.destdir, cfg.root))

    local function sync_lib(name)
        sh(string.format("cp -P %s/lib/lib%s.s* %s/lib/", cfg.destdir, name, cfg.root))
        sh(string.format("cp -P %s/lib/lib%s.a %s/lib/ 2>/dev/null || true", cfg.destdir, name, cfg.root))
    end
    local libs = {"c", "gcc_s", "gcc", "util", "crypt", "curses", "terminfo"}
    for _, l in ipairs(libs) do sync_lib(l) end
    sh(string.format("cp -P %s/usr/lib/libgcc_s.* %s/lib/ 2>/dev/null || true", cfg.destdir, cfg.root))
end

tasks["coreutils"] = function()
    step("Building Core & Admin Utils")
    local dirs = {"/bin", "/sbin", "/usr/bin", "/usr/sbin", "/usr/share/man/man1"}
    for _, dir in ipairs(dirs) do os.execute(string.format("mkdir -p %s%s", cfg.root, dir)) end

    local path_map = {
        ls = "/bin/", cp = "/bin/", mv = "/bin/", rm = "/bin/",
        cat = "/bin/", echo = "/bin/", pwd = "/bin/", mkdir = "/bin/",
        sh = "/bin/", date = "/bin/", chmod = "/bin/", hostname = "/bin/",
        ed = "/bin/", login = "/sbin/", smat = "/sbin/", lsdsk = "/bin/"
    }
    local special_libs = {
        login = "-lcrypt",
        smat  = "-lcurses -llua -lm -lterminfo",
        lsdsk = "-lprop"
    }

    local function compile_util(src, bin, extra_ld)
        local name = bin:match("([^/]+)$")
        if cfg.without[name] then return end
        if needs_update(src, bin) then
            local libs = special_libs[name] or ""
            local ldflags = string.format("-lm %s", libs)
            sh(string.format("cc -Isrc/include -I%s/include -O2 -o %s %s %s %s", 
                cfg.inst, bin, src, ldflags, extra_ld or ""))
        end
    end

    local categories = {"core", "adm", "textproc"}
    for _, cat in ipairs(categories) do
        local base_dir = "src/cmd/" .. cat
        local handle = io.popen("ls -F " .. base_dir .. " 2>/dev/null")
        if handle then
            for entry in handle:lines() do
                if entry:match("/$") then
                    local name = entry:sub(1, -2)
                    if not cfg.without[name] then
                        local subdir = base_dir .. "/" .. name
                        local target_dir = path_map[name] or "/usr/bin/"
                        if io.open(subdir .. "/Makefile", "r") then
                            local libs = special_libs[name]
                            local env = libs and string.format("LDFLAGS='%s' ", libs) or ""
                       	    sh(string.format("%s %s -C %s DESTDIR=%s install", env, "gmake", subdir, cfg.root))
		    	end
                    end
                elseif entry:match("%.c$") then
                    local name = entry:match("([^/]+)%.c$")
                    local target_dir = path_map[name] or "/usr/bin/"
                    os.execute("mkdir -p " .. cfg.root .. target_dir)
                    compile_util(base_dir .. "/" .. entry, cfg.root .. target_dir .. name)
                end
            end
            handle:close()
        end
    end     
    -- sh("chflags schg " .. cfg.root .. "/sbin/login")
end

tasks["kernel"] = function()
    step(string.format("Building Kayoko Kernel [%s]", cfg.kernel))
    local conf_path = string.format("%s/sys/arch/%s/conf/%s", cfg.src, cfg.arch, cfg.kernel)
    if not io.open(conf_path, "r") then
        sh(string.format("cp %s/sys/arch/%s/conf/GENERIC %s", cfg.src, cfg.arch, conf_path))
    end
    sh(string.format("cd %s && ./build.sh -m %s -T %s -O %s kernel=%s",
        cfg.src, cfg.arch, cfg.tools, cfg.obj, cfg.kernel))
    local kernel_obj = string.format("%s/sys/arch/%s/compile/%s/netbsd", cfg.obj, cfg.arch, cfg.kernel)
    sh(string.format("cp %s %s/unix", kernel_obj, cfg.root))
end

-- --- 5. Execution Logic ---

local run_list = {}
for i=1, #arg do 
    if not arg[i]:match("^-") then table.insert(run_list, arg[i]) end 
end

if #run_list == 0 then 
    print(ui.bold .. "Usage: ./build.lua [--without-name] [tasks]" .. ui.reset)
    os.exit(1)
end

if run_list[1] == "all" then
    run_list = {"init", "branding", "tools", "base", "coreutils", "kernel"}
end

for _, task_name in ipairs(run_list) do
    if tasks[task_name] then tasks[task_name]() end
end

print(ui.bold .. "--> Build Sequence Complete." .. ui.reset)
