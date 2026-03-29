main:
	lua $(PWD)/lpts.lua

timed:
	time lua $(PWD)/lpts.lua

json:
	lua $(PWD)/lpts.lua >/dev/null && jq < lpts.json
