-- MetadataParenthesis.lua

-- Exports song metadata from SkauTan library (one folder above self) and looks at what is parenthesized




local sqlite = require("lsqlite3")
local db = assert(sqlite.open("../SkauTan.sqlite"))


--- Stores all the values within parentheses, brackets and squigglies
-- Used as a "set": gValues["bracketedValue"] = true
local gValues = {}


db:exec("SELECT FileName FROM SongFiles",
	function(aUserData, aColumns, aValues, aColumnNames)
		aValues[1]
			:gsub("%b[]", function(inBrackets)    gValues[inBrackets]    = true end)
			:gsub("%b()", function(inParentheses) gValues[inParentheses] = true end)
			:gsub("%b{}", function(inSquigglies)  gValues[inSquigglies]  = true end)
		return 0
	end
)

-- Sort the values:
local gValuesArr = {}
for v, _ in pairs(gValues) do
	table.insert(gValuesArr, v)
end
table.sort(gValuesArr)

-- Print all the values:
for _, v in ipairs(gValuesArr) do
	print(v)
end

print("Done")
