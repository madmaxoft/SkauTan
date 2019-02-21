--[[
Tests the tempo detection by running it on multiple files and comparing the detected tempo with the tempo
stored in the files' tags

Expects the location of TempoDetectCmd on command line, as well as a filelist.
Usage:
MultiTest.lua [-d path/to/TempoDetectCmd] [-l path/to/filelist.txt]

If -d is not given, uses BeatDetectCmd[.exe] by default.
If -l is not given, processes filelist.txt by default
--]]





local isWindows = (string.sub(package.config or "", 1, 1) == "\\")
local tempoDetectCmdExe = isWindows and "TempoDetectCmd.exe" or "TempoDetectCmd"





--- Returns true if the two values are close enough
-- The values needn't be valid number at all, returns false in such a case
local function isCloseEnoughTempo(aTempo1, aTempo2)
	if not(aTempo1) or not(aTempo2) then
		return false
	end
	if (math.abs(aTempo1 - aTempo2) <= 1.5) then
		return true
	end
	return false
end





--- Processes the specified list file
-- Runs the detection on each file specified in the list
-- Stores entries in aResults, a dict table of fileName -> { tempo = ..., id3Tag = ..., }, as returned from TempoDetectCmd
local function processListFile(aListFileName, aResults, aFnDetectMPM)
	assert(type(aListFileName) == "string")
	assert(type(aResults) == "table")

	-- Run the detection:
	print("Running detection on list file " .. aListFileName)
	local cmdLine = tempoDetectCmdExe .. " -i -l \"" .. aListFileName .. "\""
	local detectionResult = assert(io.popen(cmdLine)):read("*a")
	if (not(detectionResult) or (detectionResult == "")) then
		error("Received empty detection result from TempoDetectCmd")
	end
	local det = assert(loadstring(detectionResult), "Failed to parse detection result")
	det = assert(det(), "Failed to load detection result")
	assert(type(det) == "table", "Detection result is in wrong format")

	-- Merge the results into aResults:
	for fnam, res in pairs(det) do
		if (aResults[fnam]) then
			print("Duplicate file in list: " .. tostring(fnam))
		end
		aResults[fnam] = res
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
			if (i > numParams) then
				print("Invalid commandline argument: -d requires a following argument specifying the TempoDetectCmd.exe executable location.")
				return res
			end
			tempoDetectCmdExe = aParams[i]
		elseif ((arg == "-l") or (arg == "-L")) then
			i = i + 1
			if (i > numParams) then
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





--- Collects the statistics over the entire result set
local function collectStatistics(aResults)
	assert(type(aResults) == "table")

	local numFiles = 0
	local numFailed = 0
	local numSucceeded = 0
	local numUnknown = 0
	local failedFiles = {}
	local unknownFiles = {}
	for fnam, res in pairs(aResults) do
		numFiles = numFiles + 1
		local mpmTag = tonumber(res.parsedID3Tag.mpm)
		if (isCloseEnoughTempo(mpmTag, res.tempo)) then
			numSucceeded = numSucceeded + 1
		else
			if (mpmTag) then
				numFailed = numFailed + 1
				failedFiles[numFailed] = fnam
			else
				numUnknown = numUnknown + 1
				unknownFiles[numUnknown] = fnam
			end
		end
	end
	return {
		numFiles = numFiles,
		numSucceeded = numSucceeded,
		numFailed = numFailed,
		numUnknown = numUnknown,
		failedFiles = failedFiles,
		unknownFiles = unknownFiles,
	}
end





--- Prints the statistics collected by collectStatistics()
local function printStatistics(aStats, aResults)
	assert(type(aStats) == "table")

	local succPct    = math.floor(1000 * aStats.numSucceeded / aStats.numFiles + 0.5) / 10
	local failedPct  = math.floor(1000 * aStats.numFailed    / aStats.numFiles + 0.5) / 10
	local unknownPct = math.floor(1000 * aStats.numUnknown   / aStats.numFiles + 0.5) / 10
	print("Total files processed: " .. aStats.numFiles)
	print("Successful detections: " .. aStats.numSucceeded .. "(" .. succPct    .. " %)")
	print("Failed detections:     " .. aStats.numFailed ..    "(" .. failedPct  .. " %)")
	print("Unknown detections:    " .. aStats.numUnknown ..   "(" .. unknownPct .. " %)")

	if (aStats.numFailed > 0) then
		print("Failed files:")
		for _, fnam in ipairs(aStats.failedFiles) do
			print("  " .. fnam)
			print("    Tag: " .. tostring(aResults[fnam].parsedID3Tag.mpm) .. "; detected: " .. aResults[fnam].tempo)
		end
	end

	if (aStats.numUnknown > 0) then
		print("Files with unknown tempo:")
		for _, fnam in ipairs(aStats.unknownFiles) do
			print("  " .. fnam)
			print("    Tag: " .. tostring(aResults[fnam].parsedID3Tag.mpm) .. "; detected: " .. aResults[fnam].tempo)
		end
	end
end





local res = processParams({...})
local stats = collectStatistics(res)
printStatistics(stats, res)
