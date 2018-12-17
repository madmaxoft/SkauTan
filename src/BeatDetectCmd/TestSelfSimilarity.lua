--[[
Uses BeatDetectCmd to detect individual beats, then calculates the self-similarity on them.

Expects the location of BeatDetectCmd on command line, as well as a filelist.
Usage:
TestSelfSimilarity.lua [-d path/to/BeatDetectCmd] [-l path/to/filelist.txt]

If -d is not given, uses BeatDetectCmd[.exe] by default.
If -l is not given, processes filelist.txt by default
Multiple parameters of the same kind may be given, they will be processed in the order they appear.

Tested algorithms:
  - Bucket offsets by MPM
		- Add up all contributions from min to max, weighted towards exact (50 % success rate)
		- Search for nearest match, add weighted by distance from exact match (50 % success rate)
	- Test each individual offset
		- Add one point for exact match and half point for off-by-one match (37 / 60 success rate)
			- Average points across offsets bucketed by whole-MPM (22 / 60 success rate)
		- Add one point for exact match (27 / 60 success rate)
		- For exact match, add 2 / SoundLevelDiff points (25 / 60 success rate)
		- As above, and for off-by-one match, add 1 / SoundLevelDiff points (29 / 60 success rate)
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
-- The inverse function is stridesToMPM()
local function mpmToStrides(aMPM)
	return math.floor(5625 / aMPM)
end





--- Converts the number of strides that make up a time interval, into the corresponding MPM, rounded to 2 decimal places
-- The inverse function is mpmToStrides()
local function stridesToMPM(aStrides)
	return math.floor(562500 / aStrides + 50) / 100
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





--- Runs BeatDetect on the specified song file
-- Returns the table produced by BeatDetectCmd
-- Caches the resulting table into a Lua file with the same base name and ".lua" extension
-- If the cache file is found, the detection is not run and the file is loaded instead (to speed up repeated runs)
local function runBeatDetect(aSongFileName)
	local cacheFileName = aSongFileName:gsub("%.....?", ".lua")  --Replace any 3- or 4-char extension with ".lua"
	local cacheFile = io.open(cacheFileName, "r")
	if (cacheFile) then
		print("Using cache file " .. cacheFileName)
		local cached = cacheFile:read("*a")
		cacheFile:close()
		return assert(loadstring(cached))()
	end

	-- Run the beat detection:
	local cmdLine = beatDetectCmdExe .. " -i -w " .. WINDOW_SIZE .. " -s " .. STRIDE .. " \"" .. aSongFileName .. "\""
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





--- Returns averages of similarity across MPM buckets
-- aSimilarity is is an array-table of { mpm = ..., similarity = ...}
-- Groups the similarities by the same whole-MPM, then calculates the average of the group
-- Returns an array-table of { mpm = ..., similarity = ...}
local function bucketizeSimilarityByMPM(aSimilarity)
	assert(type(aSimilarity) == "table")

	local grouped = {}
	for _, sim in ipairs(aSimilarity) do
		local fl = math.floor(sim.mpm)
		grouped[fl] = grouped[fl] or {sum = 0, count = 0}
		grouped[fl].sum = grouped[fl].sum + sim.similarity
		grouped[fl].count = grouped[fl].count + 1
	end

	local res = {}
	local n = 1
	for mpm, group in pairs(grouped) do
		res[n] = { mpm = mpm, similarity = group.sum / group.count }
		n = n + 1
	end
	return res
end





--- Runs the full detection and calculation on the specified file
-- Returns the results as a dict table, { storedMPM = ..., detectedMPM = ... }
local function runOnFile(aFileName)
	local det = runBeatDetect(aFileName)

	-- Calculate the self-similarity, sort from best:
	local similarity = {}
	local n = 1
	local beats = prepareBeats(det.beats)
	local minMPM, maxMPM = getMPMRangeForGenre(det.parsedID3Tag.genre)
	local minOfs = math.floor(mpmToStrides(maxMPM))
	local maxOfs = math.ceil(mpmToStrides(minMPM))
	for ofs = minOfs, maxOfs do
		local sim = calcSimilarity(beats, det.beats, ofs)
		similarity[n] = {mpm = stridesToMPM(ofs), similarity = sim}
		n = n + 1
	end
	-- similarity = bucketizeSimilarityByMPM(similarity)
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
		aResults[line] = runOnFile(line)
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
		if (isCloseEnoughMPM(r.storedMPM, r.detectedMPM)) then
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
