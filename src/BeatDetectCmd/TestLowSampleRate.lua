--[[

Tests beat detection using low samplerate.
Outputs the detected and stored MPM for each input file, and overall statistics, in human-readable format

Expects the location of BeatDetectCmd on command line, as well as a filelist.
Usage:
TestLowSampleRate.lua [-d path/to/BeatDetectCmd] [-a Algorithm] [-l path/to/filelist.txt]

If -d is not given, uses BeatDetectCmd[.exe] by default.
If -a is not given, "simBeats" is used by default.
If -l is not given, processes filelist.txt by default
Multiple parameters of the same kind may be given, they will be processed in the order they appear.
The supported algorithms are:
	1, "simBeats"       - Self-similarity on detected beats (detectSelfSimilarityBeats())
	2, "simLevels"      - Self-similarity on soundlevels   (detectSelfSimilarityLevels())
	3, "divBeats"       - Beat division (detectBeatDivision())
	4, "simBeatsWeight" - Self-similarity on detected beats, consider weights (detectSelfSimilarityBeatsWeight())
	5, "comboBeats"     - Combine simBeats, simBeatsWeight and divBeats


Tested algorithms / parameters:
	- Use self-similarity matching for beats
		- -a 1 -r 500 -w 11 -s 8  -p 19 [86 % success rate, 1.6 songs / sec, 13.3 songs / sec cached]
		- -a 1 -r 500 -w 11 -s 11 -p 19 [82 % success rate]
		- -a 3 -r 500 -w 11 -s 8  -p 19 [87 % success rate, 1.6 songs / sec]
		- -a 3 -r 500 -w 11 -s 8  -p 13 [88 % success rate]
		- -a 3 -r 500 -w 11 -s 8  -p 13 -n 31 [88 % success rate]
		- -a 3 -r 500 -w 11 -s 8  -p 13 -n 31, crop 10 % [89 % success rate]
	- Use self-similarity matching for beats, consider beat weights
		- -a 3 -r 500 -w 11 -s 8  -p 13 -n 31, use beats' weights as 1 / diff [57 % success rate]
		- -a 3 -r 500 -w 11 -s 8  -p 13 -n 31, use beats' weights as maxWeight / diff [90 % success rate]
		- -a 3 -r 500 -w 11 -s 8  -p 13 -n 31, use beats' weights as maxWeight / diff, crop 10 % [90 % succes rate, +5% runtime)
	- Use self-similarity matching for levels
		- -a 3 -r 500 -w 11 -s 8 [30 % success rate, 0.01 songs / sec]
	- Use beat division (pick start and stop beats, guess the biggest number that divides them into equal beats that matches the existing beats)
		- -a 3 -r 500 -w 11 -s 8 -p 13, crop 1/5 off each end [32 % success rate]
		- -a 3 -r 500 -w 11 -s 8 -p 13, crop 10 %, refine end beats by weight (20) [43 % success rate]
	- Combine simBeats, divBeats and simBeatsWeight
		- { 89 %, 90 %, 43 % } -> 81 % success rate
--]]





local isWindows = (string.sub(package.config or "", 1, 1) == "\\")
local beatDetectCmdExe = isWindows and "BeatDetectCmd.exe" or "BeatDetectCmd"

local gNumHistogramBuckets = 30






local function assert(aValue, aMsg, ...)
	if not(aValue) then
		-- If there's a debugger attached, print the message and hit a breakpoint so that the assertion can be inspected:
		local isOK, mobdebug = pcall(require, "mobdebug")
		if (isOK) then
			if (aMsg) then
				io.stderr:write(aMsg .. "\n")
			end
			io.stderr:write(debug.traceback() .. "\n")
			io.stderr:flush()
			mobdebug.pause()
		end
		error(aMsg)
	end
	return aValue, aMsg, ...
end





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
local function runBeatDetect(aSongFileName, aWindowSize, aStride, aSampleRate, aLevelPeak, aNormalizeWindow)
	assert(type(aSongFileName) == "string")
	assert(type(aWindowSize) == "number")
	assert(type(aStride) == "number")
	assert(type(aSampleRate) == "number")
	assert(type(aLevelPeak) == "number")
	assert((aNormalizeWindow == nil) or (type(aNormalizeWindow) == "number"))

	local cacheExt
	if (aNormalizeWindow) then
		cacheExt = string.format("_w%d_s%d_r%d_p%d_n%d.lua", aWindowSize, aStride, aSampleRate, aLevelPeak, aNormalizeWindow)
	else
		cacheExt = string.format("_w%d_s%d_r%d_p%d.lua", aWindowSize, aStride, aSampleRate, aLevelPeak)
	end
	local cacheFileName = aSongFileName:gsub("%.....?$", cacheExt)
	local cacheFile = io.open(cacheFileName, "r")
	if (cacheFile) then
		print("Using cache file " .. cacheFileName)
		local cached = cacheFile:read("*a")
		cacheFile:close()
		local res = loadstring(cached)
		if (res) then
			local isOK
			isOK, res = pcall(res)
			if ((isOK) and (type(res) == "table")) then
				return res
			end
		end
		print("Failed to load cache file, running detection again")
	end

	-- Run the beat detection:
	local normalization = ""
	if (aNormalizeWindow) then
		normalization = " -n " .. aNormalizeWindow
	end
	local cmdLine = beatDetectCmdExe .. " -i -a 3 -w " .. aWindowSize .. normalization .. " -s " .. aStride .. " -r " .. aSampleRate .. " -p " .. aLevelPeak .. " \"" .. aSongFileName .. "\""
	local detectionResult = assert(io.popen(cmdLine)):read("*a")
	if (not(detectionResult) or (detectionResult == "")) then
		error("Received empty detection result from BeatDetectCmd")
	end
	local det = assert(loadstring(detectionResult), "Failed to parse detection result")
	det = assert(det(), "Failed to load detection result")
	assert(type(det) == "table", "Detection result is in wrong format")

	-- Store the result in the cache file for the next time:
	cacheFile = io.open(cacheFileName, "w")
	if (cacheFile) then
		cacheFile:write(detectionResult)
		cacheFile:close()
	end
	return det
end





--- Prepares the beats data for easier implementation of the self-similarity calculation
-- aBeats is the beat structure returned from BeatDetectCmd ({{beatIdx, soundLevel, beatWeight}, ...})
-- Returns a dist-table of {beatIdx = { beatIdx, soundLevel, beatWeight} }, and the number (maxWeight)
local function prepareBeats(aBeats)
	assert(type(aBeats) == "table")
	assert(type(aBeats[1]) == "table")
	assert(type(aBeats[1][1]) == "number")

	local res = {}
	local maxWeight = aBeats[1][3]
	for _, v in ipairs(aBeats) do
		res[v[1]] = v
		if (v[3] > maxWeight) then
			maxWeight = v[3]
		end
	end
	return res, maxWeight
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
	return math.floor(mpm * 100 + 0.5) / 100
end





--- Calculates the beats' similarity with itself at the specified offset
-- aBeatsDict is the dict-table of { beatIdx = soundLevel }, returned from prepareBeats()
-- aOffset is the offset to calculate the similarity for
-- Returns a single number, the calculated similarity
local function calcSimilarityBeats(aBeatsDict, aOffset)
	assert(type(aBeatsDict) == "table")
	assert(type(aOffset) == "number")

	local res = 0
	for beatIdx, soundLevel in pairs(aBeatsDict) do
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





--- Calculates the beats' similarity with itself at the specified offset, taking the beats' weights into consideration
-- aBeatsDict is the dict-table of { beatIdx = {beatIdx, soundLevel, beatWeight} }, returned from prepareBeats()
-- aOffset is the offset to calculate the similarity for
-- Returns a single number, the calculated similarity
local function calcSimilarityBeatsWeight(aBeatsDict, aOffset, aMaxWeight)
	assert(type(aBeatsDict) == "table")
	assert(type(aOffset) == "number")
	assert(type(aMaxWeight) == "number")

	local res = 0
	for beatIdx, beatDesc in pairs(aBeatsDict) do
		local idx = aOffset + beatIdx
		if (aBeatsDict[idx]) then
			res = res + aMaxWeight - math.abs(beatDesc[3] - aBeatsDict[idx][3])
		elseif (aBeatsDict[idx + 1]) then
			res = res + (aMaxWeight - math.abs(beatDesc[3] - aBeatsDict[idx + 1][3])) / 2
		elseif (aBeatsDict[idx - 1]) then
			res = res + (aMaxWeight - math.abs(beatDesc[3] - aBeatsDict[idx - 1][3])) / 2
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





--- Crops both ends of the beat array at the specified time-ratios
-- aBeats is an array-table of { timestamp, soundlevel, beatStrength }, as returned from BDC, sorted by timestamp
-- aLowRatio and aHighRatio are ratios of the beats' timestamps against the last beat's timestamp
-- Returns a new beat array that contains only the cropped beats
local function cropBeats(aBeats, aLowRatio, aHighRatio)
	assert(type(aBeats) == "table")
	assert(type(aBeats[1]) == "table")
	assert(type(aBeats[1][1]) == "number")
	assert(type(aLowRatio) == "number")
	assert(type(aHighRatio) == "number")
	assert(aLowRatio >= 0)
	assert(aHighRatio <= 1)
	assert(aLowRatio < aHighRatio)

	local firstBeatTS = aBeats[1][1]
	local lastBeatIdx = #aBeats
	local lastBeatTS = aBeats[lastBeatIdx][1]
	local tsDiff = lastBeatTS - firstBeatTS
	local lowTS = firstBeatTS + aLowRatio * tsDiff
	local highTS = firstBeatTS + aHighRatio * tsDiff
	local res = {}
	local n = 1
	for _, beat in ipairs(aBeats) do
		if (beat[1] > highTS) then
			-- Already past the high-mark, all beats processed
			return res
		end
		if (beat[1] >= lowTS) then
			res[n] = beat
			n = n + 1
		end
	end
	return res
end





--- Crops both ends of the beat array at the specified time-ratios
-- aBeats is an array-table of { timestamp, soundlevel, beatStrength }, as returned from BDC, sorted by timestamp
-- aLowRatio and aHighRatio are ratios of the beats' timestamps against the last beat's timestamp
-- Returns a new beat array that contains only the cropped beats
-- Picks the starting and ending beats to be strong beats (crops even more)
local function cropBeatsAtStrongBounds(aBeats, aLowRatio, aHighRatio)
	assert(type(aBeats) == "table")
	assert(type(aBeats[1]) == "table")
	assert(type(aBeats[1][1]) == "number")
	assert(type(aLowRatio) == "number")
	assert(type(aHighRatio) == "number")
	assert(aLowRatio >= 0)
	assert(aHighRatio <= 1)
	assert(aLowRatio < aHighRatio)

	-- Search for the indices of the first and last beats in the specified timestamp range:
	local firstBeatTS = aBeats[1][1]
	local lastBeatIdx = #aBeats
	local lastBeatTS = aBeats[lastBeatIdx][1]
	local tsDiff = lastBeatTS - firstBeatTS
	local lowTS = firstBeatTS + aLowRatio * tsDiff
	local highTS = firstBeatTS + aHighRatio * tsDiff
	local lowIdx = 1
	local highIdx = #aBeats
	for idx, beat in ipairs(aBeats) do
		if (beat[1] < lowTS) then
			lowIdx = idx
		end
		if (beat[1] > highTS) then
			highIdx = idx
			break
		end
	end

	-- Find a close strong beat for the border indices:
	for i = lowIdx + 1, lowIdx + 20 do
		if (i >= highIdx) then
			break
		end
		if (aBeats[i][3] > aBeats[lowIdx][3]) then
			-- This is a stronger beat, use it instead:
			lowIdx = i
		end
	end
	for i = highIdx - 20, highIdx - 1 do
		if (i > 0) then
			if (aBeats[i][3] > aBeats[highIdx][3]) then
				-- This is a stronger beat, use it instead:
				highIdx = i
			end
		end
	end

	-- Return the cropped array:
	local res = {}
	local n = 1
	for idx = lowIdx, highIdx do
		res[n] = aBeats[idx]
		n = n + 1
	end
	return res
end





--- Detects MPM by self-similarity from the specified BDC result's beats, using only beat existence as criteria
-- Returns the detected MPM, the second-best guess and the certainty of the guess ([0 .. 1])
local function detectSelfSimilarityBeats(aBDCResults, aStride, aSampleRate)
	assert(type(aBDCResults) == "table")
	assert(type(aStride) == "number")
	assert(type(aSampleRate) == "number")

	local beats = prepareBeats(cropBeats(aBDCResults.beats, 0.1, 0.9))
	local minMPM, maxMPM = getMPMRangeForGenre(aBDCResults.parsedID3Tag.genre)
	local minOfs = math.floor(mpmToStrides(maxMPM, aStride, aSampleRate))
	local maxOfs = math.ceil(mpmToStrides(minMPM, aStride, aSampleRate))
	local similarities = {}
	local n = 1
	for ofs = minOfs, maxOfs do
		local sim = calcSimilarityBeats(beats, ofs)
		similarities[n] = {offset = ofs, mpm = stridesToMPM(ofs, aStride, aSampleRate), similarity = sim}
		n = n + 1
	end

	-- Find the second-best MPM (that isn't close enough to the best MPM):
	table.sort(similarities,
		function (aSim1, aSim2)
			return (aSim1.similarity > aSim2.similarity)
		end
	)
	local bestMPM = similarities[1].mpm
	local bestSimilarity = similarities[1].similarity
	for _, sim in ipairs(similarities) do
		if not(isCloseEnoughMPM(bestMPM, sim.mpm)) then
			return bestMPM, sim.mpm, 1 - sim.similarity / bestSimilarity
		end
	end

	-- There is no second-best MPM (should not happen):
	return bestMPM, 0, 1
end





--- Returns a single number representing the self-similarity of the input array of levels with the specified offset into self
local function calcSimilarityLevels(aLevels, aOffset)
	assert(type(aLevels) == "table")
	assert(type(aLevels[1]) == "number")
	assert(type(aOffset) == "number")

	local res = 0
	local numLevels = #aLevels
	local abs = math.abs
	for i = 1, numLevels - aOffset - 1 do
		local v1 = aLevels[i]
		local v2 = aLevels[i + aOffset]
		local sim = 100 / (abs(v1 - v2) + 1)
		res = res + sim
	end
	return res
end





--- Normalizes the sound levels against a local average
-- aRawLevels is an array-table of the sound levels
-- aWindowSize is the number of samples across which each level is normalized
-- Returns an array-table of numbers, each representing the normalized input
-- The returned table is shorter than aRawlevels table by aWindowSize elements
-- Normalization doesn't divide by exact average, but avoid one division in each iteration by dividing by the sum instead
-- so the results are not normalized towards 1, but rather towards aWindowSize; shouldn't matter to the final result
local function locallyNormalizeLevels(aRawLevels, aWindowSize)
	assert(type(aRawLevels) == "table")
	assert(type(aRawLevels[1]) == "number")
	assert(type(aWindowSize) == "number")

	local res = {}
	local n = 2
	local sum = 1  -- To force division with a divisor always larger than 0
	local halfSize = math.floor(aWindowSize / 2)
	for i = 1, aWindowSize do
		sum = sum + aRawLevels[i]
	end
	res[1] = aRawLevels[halfSize] / sum
	for i = 1, #aRawLevels - aWindowSize - 1 do
		res[n] = aRawLevels[i + halfSize] / sum
		n = n + 1
		sum = sum + aRawLevels[i + aWindowSize + 1] - aRawLevels[i]
	end
	return res
end





--- Detects the MPM through self-similarity from the specified BDC result's soundlevels
-- Returns the detected MPM, the second-best guess and the certainty of the guess ([0 .. 1])
local function detectSelfSimilarityLevels(aBDCResults, aStride, aSampleRate)
	assert(type(aBDCResults) == "table")
	assert(type(aStride) == "number")
	assert(type(aSampleRate) == "number")

	local minMPM, maxMPM = getMPMRangeForGenre(aBDCResults.parsedID3Tag.genre)
	local minOfs = math.floor(mpmToStrides(maxMPM, aStride, aSampleRate))
	local maxOfs = math.ceil(mpmToStrides(minMPM, aStride, aSampleRate))
	local normalizedLevels = locallyNormalizeLevels(aBDCResults.soundLevels, 22)
	local similarities = {}
	local n = 1
	for ofs = minOfs, maxOfs do
		local sim = calcSimilarityLevels(normalizedLevels, ofs)
		similarities[n] = {offset = ofs, mpm = stridesToMPM(ofs, aStride, aSampleRate), similarity = sim}
		n = n + 1
	end

	-- Find the second-best MPM (that isn't close enough to the best MPM):
	table.sort(similarities,
		function (aSim1, aSim2)
			return (aSim1.similarity > aSim2.similarity)
		end
	)
	local bestMPM = similarities[1].mpm
	local bestSimilarity = similarities[1].similarity
	for _, sim in ipairs(similarities) do
		if not(isCloseEnoughMPM(bestMPM, sim.mpm)) then
			return bestMPM, sim.mpm, 1 - sim.similarity / bestSimilarity
		end
	end

	-- There is no second-best MPM:
	return bestMPM, 0, 1
end





--- Calculates how well the beats match their division
local function calcBeatDivisionMatch(aBeatsDict, aStartTS, aEndTS, aNumDivisions)
	assert(type(aBeatsDict) == "table")
	assert(type(aStartTS) == "number")
	assert(type(aEndTS) == "number")
	assert(aStartTS < aEndTS)
	assert(type(aNumDivisions) == "number")
	assert(aNumDivisions > 0)

	local tsDiff = (aEndTS - aStartTS) / aNumDivisions
	local res = 0
	local floor = math.floor
	for i = 1, aNumDivisions - 1 do
		local ts = floor(aStartTS + tsDiff * i + 0.5)
		if (aBeatsDict[ts]) then
			res = res + aBeatsDict[ts][3]
		elseif (aBeatsDict[ts - 1]) then
			res = res + aBeatsDict[ts - 1][3] / 2
		elseif (aBeatsDict[ts + 1]) then
			res = res + aBeatsDict[ts + 1][3] / 2
		end
	end
	return res / aNumDivisions
end





--- Map of lcGenre -> true for genres that should attempt the first BPM by three, rather than two
local gFoldTriplets =
{
	["sw"] = true,
	["vw"] = true,
}

--- Folds the BPM into MPM, using hints in min / max MPM, and genre
-- Returns the folded MPM
local function foldBPM(aBPM, aMinMPM, aMaxMPM, aGenre)
	assert(type(aBPM) == "number")
	assert(type(aMinMPM) == "number")
	assert(type(aMaxMPM) == "number")
	assert(aMinMPM < aMaxMPM)
	assert((aGenre == nil) or (type(aGenre) == "string"))

	if ((aBPM >= aMinMPM) and (aBPM <= aMaxMPM)) then
		return aBPM
	end

	-- If the BPM is lower than minimum, multiply by two
	if (aBPM < aMinMPM) then
		while (aBPM < aMinMPM) do
			aBPM = aBPM * 2
		end
		return aBPM
	end

	-- The BPM is more than max, first try folding by three for eligible genres:
	if (gFoldTriplets[string.lower(aGenre or "")]) then
		aBPM = aBPM / 3
		if (aBPM <= aMaxMPM) then
			return aBPM
		end
	end

	while (aBPM > aMaxMPM) do
		aBPM = aBPM / 2
	end
	return aBPM
end





--- Detects the MPM through beat division
-- Crops the beats off of both ends, then tries to pick the highest number that divides
-- the beats in between into equal parts that each have a beat at each end.
-- Returns the detected MPM, the second-best guess and the certainty of the guess ([0 .. 1])
local function detectBeatDivision(aBDCResults, aStride, aSampleRate)
	assert(type(aBDCResults) == "table")
	assert(type(aStride) == "number")
	assert(type(aSampleRate) == "number")

	local genre = aBDCResults.parsedID3Tag.genre
	local minMPM, maxMPM = getMPMRangeForGenre(genre)
	local croppedBeats = cropBeatsAtStrongBounds(aBDCResults.beats, 0.1, 0.9)
	local beatsDict = prepareBeats(croppedBeats)
	local numCropped = #croppedBeats
	local histogram = {}
	local n = 1
	local startTS = croppedBeats[1][1]
	local endTS = croppedBeats[#croppedBeats][1]
	local diffTS = endTS - startTS
	local minDiv = math.floor(diffTS / mpmToStrides(minMPM, aStride, aSampleRate))
	local maxDiv = math.floor(diffTS / mpmToStrides(maxMPM, aStride, aSampleRate))
	local best = {match = 0}
	for i = minDiv, maxDiv do
		histogram[n] = {division = i, match = calcBeatDivisionMatch(beatsDict, startTS, endTS, i)}
		histogram[n].mpm = foldBPM(stridesToMPM((endTS - startTS) / histogram[n].division, aStride, aSampleRate), minMPM, maxMPM, genre)
		if (histogram[n].match > best.match) then
			best = histogram[n]
		end
		n = n + 1
	end

	table.sort(histogram,
		function(aHist1, aHist2)
			return (aHist1.match > aHist2.match)
		end
	)

	-- Process the results into MPMs and certainty:
	local bestMPM = foldBPM(stridesToMPM((endTS - startTS) / best.division, aStride, aSampleRate), minMPM, maxMPM, genre)
	--[[
	if not(isCloseEnoughMPM(bestMPM, aBDCResults.parsedID3Tag.mpm)) then
		-- Known failure, check the certainty manually
		print("Check the certainty manually, breakpoint here")
	end
	--]]
	for _, hist in ipairs(histogram) do
		local secondBestMPM = foldBPM(stridesToMPM((endTS - startTS) / hist.division, aStride, aSampleRate), minMPM, maxMPM, genre)
		if not(isCloseEnoughMPM(bestMPM, secondBestMPM)) then
			local certainty = 1 - hist.match / best.match
			return bestMPM, secondBestMPM, certainty
		end
	end

	-- There is no second MPM guess (should not happen):
	return bestMPM, 0, 1
end





--- Detects MPM by self-similarity from the specified BDC result's beats, taking beats' weights into consideration
-- Returns the detected MPM, the second-best guess and the certainty of the guess ([0 .. 1])
local function detectSelfSimilarityBeatsWeight(aBDCResults, aStride, aSampleRate)
	assert(type(aBDCResults) == "table")
	assert(type(aBDCResults.beats) == "table")
	assert(type(aStride) == "number")
	assert(type(aSampleRate) == "number")

	local beats, maxWeight = prepareBeats(aBDCResults.beats)
	local minMPM, maxMPM = getMPMRangeForGenre(aBDCResults.parsedID3Tag.genre)
	local minOfs = math.floor(mpmToStrides(maxMPM, aStride, aSampleRate))
	local maxOfs = math.ceil(mpmToStrides(minMPM, aStride, aSampleRate))
	local similarities = {}
	local n = 1
	for ofs = minOfs, maxOfs do
		local sim = calcSimilarityBeatsWeight(beats, ofs, maxWeight)
		similarities[n] = {offset = ofs, mpm = stridesToMPM(ofs, aStride, aSampleRate), similarity = sim}
		n = n + 1
	end

	-- Find the second-best MPM (that isn't close enough to the best MPM):
	table.sort(similarities,
		function (aSim1, aSim2)
			return (aSim1.similarity > aSim2.similarity)
		end
	)
	local bestMPM = similarities[1].mpm
	local bestSimilarity = similarities[1].similarity
	for _, sim in ipairs(similarities) do
		if not(isCloseEnoughMPM(bestMPM, sim.mpm)) then
			return bestMPM, sim.mpm, 1 - sim.similarity / bestSimilarity
		end
	end

	-- There is no second-best MPM (should not happen):
	return bestMPM, 0, 1
end





--- Detects MPM by running detectSelfSimilarityBeats() detectSelfSimilarityBeatsWeight() and detectBeatDivision(),
-- then combining the results.
-- Returns the detected MPM, the second-best guess and the certainty of the guess ([0 .. 1])
local function detectCombo(aBDCResults, aStride, aSampleRate)
	local resSimBeats = { detectSelfSimilarityBeats(aBDCResults, aStride, aSampleRate) }
	local resSimBeatsWeight = { detectSelfSimilarityBeatsWeight(aBDCResults, aStride, aSampleRate) }
	local resSimBeatDiv = { detectBeatDivision(aBDCResults, aStride, aSampleRate) }

	-- Combine the results:
	local mpms = {}
	local floor = math.floor
	local addResults = function(aRes)
		local mainMPM = floor(aRes[1] + 0.5)
		local secondMPM = floor(aRes[2] + 0.5)
		mpms[mainMPM] = (mpms[mainMPM] or 0) + aRes[3]
		mpms[secondMPM] = (mpms[secondMPM] or 0) + aRes[3] / 3
	end
	addResults(resSimBeats)
	addResults(resSimBeatsWeight)
	addResults(resSimBeatDiv)

	-- Return the highest-ranking MPM:
	local mpmArr = {}
	local n = 1
	for mpm, certainty in pairs(mpms) do
		mpmArr[n] = { mpm = mpm, certainty = certainty }
		n = n + 1
	end
	table.sort(mpmArr,
		function(aVal1, aVal2)
			return (aVal1.certainty > aVal2.certainty)
		end
	)

	return mpmArr[1].mpm, mpmArr[2].mpm, mpmArr[1].certainty
end





--- Runs the BeatDetectCmd on the specified song file, using different detection params
-- Returns a dict table of {windowSize = ..., stride = ...} -> <BeatDetectCmd results>, with a summary
local function runOnFile(aSongFileName, aFnDetectMPM)
	local sampleRate = 500
	local windowSize = 11
	local stride = 8
	local levelPeak = 13
	local normalizationWindow = 31
	local res = runBeatDetect(aSongFileName, windowSize, stride, sampleRate, levelPeak, normalizationWindow)
	res.detectedMPM, res.secondMPM, res.certainty = aFnDetectMPM(res, stride, sampleRate)
	return res
end





--- Processes the specified list file
-- Runs the detection on each file specified in the list
-- Stores entries in aResults, a dict table of fileName -> { summary = ..., {windowSize = ..., stride = ...} -> <BeatDetectCmd results> } for each song processed
local function processListFile(aListFileName, aResults, aFnDetectMPM)
	assert(type(aListFileName) == "string")
	assert(type(aResults) == "table")
	assert(type(aFnDetectMPM) == "function")

	for line in io.lines(aListFileName) do
		aResults[line] = runOnFile(line, aFnDetectMPM)
	end
end





--- The known MPM detection algorithms, keyed by their names on the commandline
local gDetectionAlgorithms =
{
	[1] = detectSelfSimilarityBeats,
	["simBeats"] = detectSelfSimilarityBeats,
	[2] = detectSelfSimilarityLevels,
	["simLevels"] = detectSelfSimilarityLevels,
	[3] = detectBeatDivision,
	["divBeats"] = detectBeatDivision,
	[4] = detectSelfSimilarityBeatsWeight,
	["simBeatsWeight"] = detectSelfSimilarityBeatsWeight,
	[5] = detectCombo,
	["comboBeats"] = detectCombo,
}

--- Processes the command line parameters
-- For each "-l" param the filelist is processed using the most recently specified BeatDetectCmd executable
-- Returns a dict table of fileName -> { summary = ..., {windowSize = ..., stride = ...} -> <BeatDetectCmd results> } for each song processed
local function processParams(aParams)
	local numParams = #aParams
	local i = 1
	local res = {}
	local hasHadFileList = false
	local fnDetectMPM = detectSelfSimilarityBeats
	while (i < numParams) do
		local arg = aParams[i]
		if ((arg == "-d") or (arg == "-D")) then
			i = i + 1
			if (i > numParams) then
				print("Invalid commandline argument: -d requires a following argument specifying the BeatDetectCmd.exe executable location.")
				return res
			end
			beatDetectCmdExe = aParams[i]
		elseif ((arg == "-l") or (arg == "-L")) then
			i = i + 1
			if (i > numParams) then
				print("Invalid commandline argument: -l requires a following argument specifying the listfile location.")
				return res
			end
			processListFile(aParams[i], res, fnDetectMPM)
			hasHadFileList = true
		elseif ((arg == "-a") or (arg == "-A")) then
			i = i + 1
			if (i > numParams) then
				print("Invalid commandline argument: -a requires a following argument specifying the algorithm to use.")
				return res
			end
			fnDetectMPM = gDetectionAlgorithms[aParams[i]]
			if not(fnDetectMPM) then
				print("Unknown detection algorithm: " .. aParams[i])
				print("  Supported algorithms:")
				for k, v in pairs(gDetectionAlgorithms) do
					if (type(k) == "string") then
						print("    " .. k)
					end
				end
				return res
			end
		end
		i = i + 1
	end
	if not(hasHadFileList) then
		processListFile("filelist.txt", res, fnDetectMPM)
	end
	return res
end





--- Updates the min and max bounds to include the specified value, too
-- aBounds is a table that has "min" and "max" values
local function updateBounds(aValue, aBounds)
	assert(type(aBounds) == "table")
	assert(type(aBounds.min) == "number")
	assert(type(aBounds.max) == "number")

	if not(aValue) then
		return
	end
	assert(type(aValue) == "number")
	if (aValue == 0) then
		print("here")
	end
	if (aValue < aBounds.min) then
		aBounds.min = aValue
	end
	if (aValue > aBounds.max) then
		aBounds.max = aValue
	end
end





local function printResults(aResults)
	assert(type(aResults) == "table")

	local numFailed = 0
	local numTotal = 0
	local successCertaintyBounds = {min = 1, max = 0}
	local failureCertaintyBounds = {min = 1, max = 0}
	for fileName, res in pairs(aResults) do
		print("File " .. fileName)
		print("\tStoredMPM: " .. (res.parsedID3Tag.mpm or "unknown"))
		print("\tDetectedMPM: " .. res.detectedMPM)
		print("\tSecondMPM: " .. (res.secondMPM or "none"))
		print("\tCertainty: " .. (res.certainty or "none"))
		if not(isCloseEnoughMPM(res.parsedID3Tag.mpm, res.detectedMPM)) then
			print("\t*** SONG FAILED ***")
			numFailed = numFailed + 1
			updateBounds(res.certainty, failureCertaintyBounds)
		else
			updateBounds(res.certainty, successCertaintyBounds)
		end
		numTotal = numTotal + 1
	end
	print("")
	print("Total songs: " .. numTotal)
	print("Total failed songs: " .. numFailed)
	local successRate = (numTotal - numFailed) / numTotal
	local successRatePct = math.floor(successRate * 10000 + 0.5) / 100
	print("Success rate: " .. successRatePct .. " %")
	print("Success certainty bounds: " .. successCertaintyBounds.min .. " .. " .. successCertaintyBounds.max)
	print("Failure certainty bounds: " .. failureCertaintyBounds.min .. " .. " .. failureCertaintyBounds.max)
end





local function calcHistograms(aResults)
	assert(type(aResults) == "table")

	local histSucc = {}
	local histFail = {}
	for i = 1, gNumHistogramBuckets do
		histSucc[i] = 0
		histFail[i] = 0
	end
	for _, res in pairs(aResults) do
		local bucket = math.floor(res.certainty * gNumHistogramBuckets + 0.5)
		if (isCloseEnoughMPM(res.parsedID3Tag.mpm, res.detectedMPM)) then
			histSucc[bucket] = (histSucc[bucket] or 0) + 1
		else
			histFail[bucket] = (histFail[bucket] or 0) + 1
		end
	end
	return histSucc, histFail
end





local function printHistogram(aHistogram)
	assert(type(aHistogram) == "table")
	assert(type(aHistogram[1]) == "number")

	local floor = math.floor
	local rep = string.rep
	local total = 0
	local max = 0
	for _, cnt in ipairs(aHistogram) do
		total = total + cnt
		if (cnt > max) then
			max = cnt
		end
	end
	local cumul = 0
	for idx, cnt in ipairs(aHistogram) do
		local valMin = floor((idx / gNumHistogramBuckets) * 100 + 0.5) / 100
		local valMax = floor(((idx + 1) / gNumHistogramBuckets) * 100 + 0.5) / 100
		cumul = cumul + cnt
		print("\t" .. valMin .. " .. " .. valMax ..
			"\t" .. cnt .. "\t" .. floor(100 * cnt / total + 0.5) ..
			" %\tcumul " .. floor(100 * cumul / total + 0.5) ..
			" %\trcumul " .. floor(100.5 - 100 * cumul / total) ..
			"\t" .. rep("#", 30 * cnt / max)
		)
	end
end





local res = processParams({...})
printResults(res)
local histSucc, histFail = calcHistograms(res)
print("Success certainty histogram:")
printHistogram(histSucc)
print("Failure certainty histogram:")
printHistogram(histFail)
