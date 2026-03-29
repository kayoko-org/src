-- 1. Basic line count
test("wc: basic line count", function()
    os.execute("printf '1\n2\n3\n' > _wc_test")
    local f = io.popen("wc -l _wc_test")
    local res = f:read("*a"):match("%d+")
    f:close()
    assert(tonumber(res) == 3, "wc -l failed to count lines")
    os.execute("rm _wc_test")
end)

-- 2. Word and byte count
test("wc: word and byte count", function()
    os.execute("printf 'hello world' > _wc_test")
    local f = io.popen("wc -w -c _wc_test")
    local words, bytes = f:read("*a"):match("(%d+)%s+(%d+)")
    f:close()
    assert(tonumber(words) == 2, "wc -w count mismatch")
    assert(tonumber(bytes) == 11, "wc -c count mismatch")
    os.execute("rm _wc_test")
end)

-- 3. Stdin handling
test("wc: stdin handling", function()
    local f = io.popen("echo 'test' | wc -l")
    local res = f:read("*a"):match("%d+")
    f:close()
    assert(tonumber(res) == 1, "wc failed to read from stdin")
end)

-- 4. Empty file handling
test("wc: empty file", function()
    os.execute("touch _empty")
    local f = io.popen("wc -l -w -c _empty")
    local l, w, c = f:read("*a"):match("(%d+)%s+(%d+)%s+(%d+)")
    f:close()
    os.execute("rm _empty")
    assert(tonumber(l) == 0 and tonumber(w) == 0 and tonumber(c) == 0)
end)

-- 5. Multibyte character count (-m)
test("wc: multibyte count (-m) (extended)", function()
    os.execute("printf 'ñ' > _multi")
    local f = io.popen("wc -m _multi 2>/dev/null")
    local res = f:read("*a")
    f:close()
    os.execute("rm _multi")

    if res and res:match("%d") then
        assert(tonumber(res:match("%d+")) == 1)
        notice("Extended flag -m supported")
    end
end)

-- 6. Multiple files and total
test("wc: multiple files", function()
    os.execute("printf 'a\n' > _f1; printf 'b\n' > _f2")
    local f = io.popen("wc -l _f1 _f2")
    local out = f:read("*a")
    f:close()
    os.execute("rm _f1 _f2")
    assert(out:match("total") and out:match("2"), "Multiple files should show total")
end)

-- 7. Longest line length (-L)
test("wc: max line length (-L) (extended)", function()
    os.execute("printf 'abc\nabcde\nf' > _long")
    local f = io.popen("wc -L _long 2>/dev/null")
    local res = f:read("*a")
    f:close()
    os.execute("rm _long")

    if res and res:match("%d") then
        assert(tonumber(res:match("%d+")) == 5)
        notice("Extended flag -L supported")
    end
end)

-- 8. No newline at EOF
test("wc: no newline at EOF (extended)", function()
    os.execute("printf 'line1\nline2' > _nonl")
    local f = io.popen("wc -l < _nonl")
    local res = f:read("*a"):match("%d+")
    f:close()
    os.execute("rm _nonl")
    assert(tonumber(res) == 1, "POSIX: only count newline characters")
end)

-- 9. Word boundary logic (tabs/spaces)
test("wc: word boundary logic (extended)", function()
    os.execute("printf 'word1\tword2  word3\nword4' > _words")
    local f = io.popen("wc -w < _words")
    local res = f:read("*a"):match("%d+")
    f:close()
    os.execute("rm _words")
    assert(tonumber(res) == 4)
end)
