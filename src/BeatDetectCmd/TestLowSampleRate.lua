--[[

Tests beat detection using low samplerate

Expects the location of BeatDetectCmd on command line, as well as a filelist.
Usage:
TestLowSampleRate.lua [-d path/to/BeatDetectCmd] [-l path/to/filelist.txt]

If -d is not given, uses BeatDetectCmd[.exe] by default.
If -l is not given, processes filelist.txt by default
Multiple parameters of the same kind may be given, they will be processed in the order they appear.


Tested algorithms / parameters:
	- Use self-similarity matching for beats
		- samplerate 500, window size 11, stride 8 [54 / 60 success rate, including bad waltzes!] in 80 seconds
		- samplerate 500, window size 11, stride 11 [52 / 60 success rate] in 80 seconds
--]]





local isWindows = (string.sub(package.config or "", 1, 1) == "\\")
local beatDetectCmdExe = isWindows and "BeatDetectCmd.exe" or "BeatDetectCmd"






--- Returns true if the two values are close enough
-- The values needn't be valid number at all, returns false in such a case
local function isCloseEnoughMPM(aMPM1, aMPM2)
	if not(aMPM1) or not(aMPM2) then
		return false
	end
	if (math.abs(aMPM1 - aMPM2) <= 1.5) then
		return true
	end
	return false
end





--- Runs BeatDetect on the specified song file
-- Returns the table produced by BeatDetectCmd
-- Caches the resulting table into a Lua file with the same base name with "_w<win>_s<stride>_r<samplerate>.lua" extension
-- If the cache file is found, the detection is not run and the cache file is loaded instead (to speed up repeated runs)
local function runBeatDetect(aSongFileName, aWindowSize, aStride, aSampleRate)
	assert(type(aSongFileName) == "string")
	assert(type(aWindowSize) == "number")
	assert(type(aStride) == "number")

	local cacheExt = "_w" .. aWindowSize .."_s" .. aStride .. "_r" .. aSampleRate .. ".lua"
	local cacheFileName = aSongFileName:gsub("%.....?", cacheExt)  --Replace any 3- or 4-char extension with ".lua"
	local cacheFile = io.open(cacheFileName, "r")
	if (cacheFile) then
		print("Using cache file " .. cacheFileName)
		local cached = cacheFile:read("*a")
		cacheFile:close()
		return assert(loadstring(cached))()
	end

	-- Run the beat detection:
	local cmdLine = beatDetectCmdExe .. " -i -w " .. aWindowSize .. " -s " .. aStride .. " -r " .. aSampleRate .. " \"" .. aSongFileName .. "\""
	local detectionResult = assert(io.popen(cmdLine)):read("*a")
	local det = assert(loadstring(detectionResult))
	det = det()

	-- Store the result in the cache file for the next time:
	cacheFile = io.open(cacheFileName, "w")
	if (cacheFile) then
		cacheFile:write(detectionResult)
		cacheFile:close()
	end
	return det
end





--- Prepares the beats data for easier implementation of the self-similarity calculation
-- aBeats is the beat structure returned from BeatDetectCmd ({{beatIdx, soundLevel}, ...})
-- Returns a dist-table of {beatIdx = soundLevel}
local function prepareBeats(aBeats)
	assert(type(aBeats) == "table")

	local res = {}
	for _, v in ipairs(aBeats) do
		res[v[1]] = v[2]
	end
	return res
end





--- Converts the mpm into (whole) number of strides that makes up the interval for that MPM
-- The inverse function is stridesToMPM()
local function mpmToStrides(aMPM, aStrideSize, aSampleRate)
	return math.floor((aSampleRate * 60 / aStrideSize) / aMPM)
end





--- Converts the number of strides that make up a time interval, into the corresponding MPM, rounded to 2 decimal places
-- The inverse function is mpmToStrides()
local function stridesToMPM(aStrides, aStrideSize, aSampleRate)
	local mpm = (aSampleRate * 60 / aStrideSize) / aStrides
	return math.floor(mpm * 100 + 50) / 100
end





--- Calculates the beats' similarity with itself at the specified offset
-- aBeatsDict is the dict-table of { beatIdx = soundLevel }, returned from prepareBeats()
-- aBeatsArr is the array table of { {beatIdx, soundLevel}, ...}, returned by BeatDetectCmd
-- aOffset is the offset to calculate the similarity for
-- Returns a single number, the calculated similarity
local function calcSimilarity(aBeatsDict, aBeatsArr, aOffset)
	assert(type(aBeatsDict) == "table")
	assert(type(aBeatsArr) == "table")
	assert(type(aOffset) == "number")

	local res = 0
	for _, beat in ipairs(aBeatsArr) do
		local beatIdx = beat[1]
		local idx = aOffset + beatIdx
		if (aBeatsDict[idx]) then
			res = res + 2
		elseif (aBeatsDict[idx + 1]) then
			res = res + 1
		elseif (aBeatsDict[idx - 1]) then
			res = res + 1
		end
	end
	return res
end





--- Dict-table of genre -> detection MPM range for the genre
-- Used in getMPMRangeForGenre() to return the correct MPM range
local mpmRangeForGenre =
{
	sw = {25, 33},
	tg = {28, 35},
	vw = {40, 65},
	sf = {25, 35},
	qs = {45, 55},

	sb = {34, 55},
	ch = {22, 35},
	ru = {17, 30},
	pd = {40, 68},
	ji = {28, 46},

	bl = {15, 30},
	po = {40, 80},  -- TODO: Fix this completely wild guess
}





--- Returns the MPM range for the specified genre
-- If the genre is not recognized, the full range of 17 .. 80 MPM is returned
local function getMPMRangeForGenre(aGenre)
	local range = mpmRangeForGenre[string.lower(aGenre or "")]
	if not(range) then
		return 17, 80
	end
	return range[1], range[2]
end





--- Returns the MPM detected by self-similarity from the specified BDC beats
local function detectSelfSimilarity(aBDCResults, aStride, aSampleRate)
	assert(type(aBDCResults) == "table")
	assert(type(aStride) == "number")
	assert(type(aSampleRate) == "number")

	local bestSimilarity = 0
	local bestMPM = 0
	local n = 1
	local beats = prepareBeats(aBDCResults.beats)
	local minMPM, maxMPM = getMPMRangeForGenre(aBDCResults.parsedID3Tag.genre)
	local minOfs = math.floor(mpmToStrides(maxMPM, aStride, aSampleRate))
	local maxOfs = math.ceil(mpmToStrides(minMPM, aStride, aSampleRate))
	for ofs = minOfs, maxOfs do
		local sim = calcSimilarity(beats, aBDCResults.beats, ofs)
		if (sim > bestSimilarity) then
			bestSimilarity = sim
			bestMPM = stridesToMPM(ofs, aStride, aSampleRate)
		end
	end
	return bestMPM
end





--- Runs the BeatDetectCmd on the specified song file, using different detection params
-- Returns a dict table of {windowSize = ..., stride = ...} -> <BeatDetectCmd results>, with a summary
local function runOnFile(aSongFileName)
	local sampleRate = 500
	local windowSize = 11
	local stride = 11
	local res = runBeatDetect(aSongFileName, windowSize, stride, sampleRate)
	res.detectedMPM = detectSelfSimilarity(res, stride, sampleRate)
	return res
end





--- Processes the specified list file
-- Runs the detection on each file specified in the list
-- Stores entries in aResults, a dict table of fileName -> { summary = ..., {windowSize = ..., stride = ...} -> <BeatDetectCmd results> } for each song processed
local function processListFile(aListFileName, aResults)
	assert(type(aListFileName) == "string")
	assert(type(aResults) == "table")

	for line in io.lines(aListFileName) do
		aResults[line] = runOnFile(line)
	end
end





--- Processes the command line parameters
-- For each "-l" param the filelist is processed using the most recently specified BeatDetectCmd executable
-- Returns a dict table of fileName -> { summary = ..., {windowSize = ..., stride = ...} -> <BeatDetectCmd results> } for each song processed
local function processParams(aParams)
	local numParams = #aParams
	local i = 1
	local res = {}
	local hasHadFileList = false
	while (i < numParams) do
		local arg = aParams[i]
		if ((arg == "-d") or (arg == "-D")) then
			i = i + 1
			if (i < numParams) then
				print("Invalid commandline argument: -d requires a following argument specifying the BeatDetectCmd.exe executable location.")
				return res
			end
			beatDetectCmdExe = aParams[i]
		elseif ((arg == "-l") or (arg == "-L")) then
			i = i + 1
			if (i < numParams) then
				print("Invalid commandline argument: -l requires a following argument specifying the listfile location.")
				return res
			end
			processListFile(aParams[i], res)
			hasHadFileList = true
		end
		i = i + 1
	end
	if not(hasHadFileList) then
		processListFile("filelist.txt", res)
	end
	return res
end





local function printResults(aResults)
	assert(type(aResults) == "table")

	local numFailed = 0
	for fileName, res in pairs(aResults) do
		print("File " .. fileName)
		print("\tStoredMPM: " .. res.parsedID3Tag.mpm or "unknown")
		print("\tDetectedMPM: " .. res.detectedMPM)
		if not(isCloseEnoughMPM(res.parsedID3Tag.mpm, res.detectedMPM)) then
			print("\t*** SONG FAILED ***")
			numFailed = numFailed + 1
		end
	end
	print("")
	print("Total failed songs: " .. numFailed)
end





local res = processParams({...})
printResults(res)
