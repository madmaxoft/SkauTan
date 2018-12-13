--[[
Uses BeatDetectCmd to detect individual beats, then calculates the self-similarity on them.

Expects the location of BeatDetectCmd on command line, as well as a filelist.
Usage:
TestSelfSimilarity.lua [-d path/to/BeatDetectCmd] [-l path/to/filelist.txt]

If -d is not given, uses BeatDetectCmd[.exe] by default.
If -l is not given, processes filelist.txt by default
Multiple parameters of the same kind may be given, they will be processed in the order they appear.

--]]




-- Adjustable parameters sent to BeatDetectTest:
local WINDOW_SIZE = 1024
local STRIDE = 512

local isWindows = (string.sub(package.config or "", 1, 1) == "\\")
local beatDetectCmdExe = isWindows and "BeatDetectCmd.exe" or "BeatDetectCmd"





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
local function mpmToStrides(aMPM)
	return math.floor(5625 / aMPM)
end





--- Calculates the beats' similarity with itself at the specified offsets
-- aBeatsDict is the dict-table of { beatIdx = soundLevel }, returned from prepareBeats()
-- aBeatsArr is the array table of { {beatIdx, soundLevel}, ...}, returned by BeatDetectCmd
-- aMinOffset is the minimum offset to consider a match
-- aExactOffset is the offset to consider the exact match
-- aMaxOffset is the maximum offset to consider a match
-- Returns a single number, the calculated similarity
local function calcSimilarity(aBeatsDict, aBeatsArr, aMinOffset, aExactOffset, aMaxOffset)
	assert(type(aBeatsDict) == "table")
	assert(type(aBeatsArr) == "table")
	assert(type(aMinOffset) == "number")
	assert(type(aExactOffset) == "number")
	assert(type(aMaxOffset) == "number")
	assert(math.floor(aMinOffset) == aMinOffset)  -- The numbers are whole
	assert(math.floor(aExactOffset) == aExactOffset)
	assert(math.floor(aMaxOffset) == aMaxOffset)

	--[[
	-- Algorithm: Add up all contributions from min to max, weighted towards exact
	local res = 0
	for k, v in pairs(aBeatsDict) do
		for ofs = aMinOffset, aExactOffset - 1 do
			if (aBeats[k + ofs]) then
				res = res + 1 / (aExactOffset + 1 - ofs)
			end
		end
		for ofs = aExactOffset, aMaxOffset do
			if (aBeats[k + ofs]) then
				res = res + 1 / (ofs - aExactOffset + 1)
			end
		end
	end
	return res
	--]]

	-- Algorithm: Search for the nearest from min to max, add only its weight relative to exact
	local res = 0
	for k, v in pairs(aBeatsDict) do
		local bestOfs
		local bestDif = aMaxOffset - aMinOffset
		for ofs = aMinOffset, aMaxOffset do
			if (aBeatsDict[k + ofs] and (math.abs(ofs - aExactOffset) < bestDif)) then
				bestOfs = ofs
				bestDif = math.abs(bestOfs - aExactOffset)
			end
		end
		if (bestOfs) then
			res = res + 1 / (bestDif + 1)
		--[[
		else
			res = res - 1  -- Penalize a missing match
		--]]
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





--- Runs detection on the specified song file
-- Returns the results as a dict table, { storedMPM = ..., detectedMPM = ... }
local function runDetection(aSongFileName)
	-- Run the beat detection:
	local cmdLine = beatDetectCmdExe .. " -i -w " .. WINDOW_SIZE .. " -s " .. STRIDE .. " \"" .. aSongFileName .. "\""
	local detectionResult = assert(io.popen(cmdLine)):read("*a")
	local det = assert(loadstring(detectionResult))
	det = det()

	-- Calculate the self-similarity, sort from best:
	local similarity = {}
	local n = 1
	local beats = prepareBeats(det.beats)
	local minMPM, maxMPM = getMPMRangeForGenre(det.parsedID3Tag.genre)
	for mpm = minMPM, maxMPM do
		local minOffset   = mpmToStrides(mpm + 0.5)
		local exactOffset = mpmToStrides(mpm)
		local maxOffset   = mpmToStrides(mpm - 0.5)
		local sim = calcSimilarity(beats, det.beats, minOffset, exactOffset, maxOffset)
		similarity[n] = {mpm = mpm, similarity = sim}
		n = n + 1
	end
	table.sort(similarity,
		function(aSim1, aSim2)
			return (aSim1.similarity > aSim2.similarity)
		end
	)

	-- Find the stored MPM's similarity in the sorted list:
	local storedMPM = (det.parsedID3Tag or {}).mpm
	local storedMPMIndex
	if (storedMPM) then
		for idx, sim in ipairs(similarity) do
			if (sim.mpm == storedMPM) then
				storedMPMIndex = idx
				break
			end
		end
	end

	-- Return the results:
	return {
		storedMPM = storedMPM,
		detectedMPM = similarity[1].mpm,
		storedMPMIndex = storedMPMIndex,
		similarity = similarity,
	}
end





--- Processes the specified list file
-- Runs the detection on each file specified in the list
-- Stores entries in aResults, a dict table of fileName -> { storedMPM = ..., detectedMPM = ... } for each song processed
local function processListFile(aListFileName, aResults)
	assert(type(aListFileName) == "string")
	assert(type(aResults) == "table")

	for line in io.lines(aListFileName) do
		aResults[line] = runDetection(line)
	end
end





--- Processes the command line parameters
-- For each "-l" param the filelist is processed using the most recently specified BeatDetectCmd executable
-- Returns a dict table of fileName -> { storedMPM = ..., detectedMPM = ..., storedMPMIndex = ... } for each song processed
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





--- Prints the summary of the processed files
-- aResults is a dict-table of fileName -> {storedMPM = ..., detectedMPM = ..., storedMPMIndex = ...}, as returned from processParams()
-- Prints the number of successful and failed detections and a per-song details
local function printSummary(aResults, aNumSeconds)
	assert(type(aResults) == "table")
	assert(type(aNumSeconds) == "number")

	-- Collect the filenames, alpha-sort:
	local fileNames = {}
	local n = 1
	for k, _ in pairs(aResults) do
		fileNames[n] = k
		n = n + 1
	end
	table.sort(fileNames)

	-- Print the per-song details:
	local numMatches = 0
	local numFails = 0
	for _, fileName in ipairs(fileNames) do
		local r = aResults[fileName]
		print(fileName)
		print("\tStoredMPM = " .. (r.storedMPM or "<none>"))
		print("\tDetectedMPM = " .. r.detectedMPM)
		local sim = r.similarity
		local diff = sim[2].similarity / sim[1].similarity
		print("\trelative similarity diff: " .. math.floor(diff * 1000) / 1000)
		if (r.storedMPM == r.detectedMPM) then
			numMatches = numMatches + 1
			print("\tmatch")
		elseif (r.storedMPM) then
			numFails = numFails + 1
			print("\t**** FAIL ****")
			---[[
			for k, v in ipairs(r.similarity) do
				print("\t" .. k .. "\t" .. v.mpm .. "\t" .. v.similarity)
			end
			--]]
		end
	end

	-- Print the summary:
	print("Total processed songs: " .. #fileNames)
	print("Matched songs: " .. numMatches)
	print("FAILED songs: " .. numFails)
	print("Time: " .. aNumSeconds .. " seconds")
end





local startTime = os.time()
local res = processParams({...})
printSummary(res, os.time() - startTime)
