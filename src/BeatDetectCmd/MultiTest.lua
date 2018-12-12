--[[

Runs BeatDetectCmd on multiple files with known MPM, checks the detected tempo against the known MPM

Usage:
MultiTest.lua [-d c:\path\to\BeatDetectCmd.exe] [-l c:\path\to\filelist.txt]

Reads the list of songfiles to run detection on from filelist.txt (each line is a single filename).
Runs the specified executable (BeatDetectCmd[.exe] in current folder if not given) on each songfile.
Multiple list files can be given. If none was given, reads filelist.txt from current folder
--]]






-- Adjustable parameters sent to BeatDetectTest:
local WINDOW_SIZE = 1024
local STRIDE = 512

local isWindows = (string.sub(package.config or "", 1, 1) == "\\")
local beatDetectCmdExe = isWindows and "BeatDetectCmd.exe" or "BeatDetectCmd"





--- Runs the detection on a single song
--[[ Returns a table of the detected values, as returned from BeatDetectCmd:
{
	fileName = "file.mp3",
	confidences = { {mpm, confidence}, ... },
	histogram = { {mpm, count}, ... },
	beats = { {beat index, sound-level}, ... },
}
--]]
local function runDetection(a_SongFileName)
	assert(type(a_SongFileName) == "string")

	local cmdLine = beatDetectCmdExe .. " -i -w " .. WINDOW_SIZE .. " -s " .. STRIDE .. " \"" .. a_SongFileName .. "\""
	local detectionResult = assert(io.popen(cmdLine)):read("*a")
	local res = assert(loadstring(detectionResult))
	return res()
end





--- Runs the detection on each file in the specified listfile
-- The results for the file are added to a_Results, a dict table of SongFileName -> {array of {mpm = .., confidence = ..}}
local function processListFile(a_ListFileName, a_Results)
	assert(type(a_ListFileName) == "string")
	assert(type(a_Results) == "table")

	for line in io.lines(a_ListFileName) do
		a_Results[line] = runDetection(line)
	end
end





--- Processes the commandline parameters given to the script
-- Returns a dict table of SongFileName -> {array of {mpm = .., confidence = ..}}
local function processParams(a_Args)
	local numArgs = #a_Args
	local i = 1
	local res = {}
	local hasHadFileList = false
	while (i < numArgs) do
		local arg = a_Args[i]
		if ((arg == "-d") or (arg == "-D")) then
			i = i + 1
			if (i < numArgs) then
				print("Invalid commandline argument: -d requires a following argument specifying the BeatDetectCmd.exe executable location.")
				return res
			end
			beatDetectCmdExe = a_Args[i]
		elseif ((arg == "-l") or (arg == "-L")) then
			i = i + 1
			if (i < numArgs) then
				print("Invalid commandline argument: -l requires a following argument specifying the listfile location.")
				return res
			end
			processListFile(a_Args[i], res)
			hasHadFileList = true
		end
		i = i + 1
	end
	if not(hasHadFileList) then
		processListFile("filelist.txt", res)
	end
	return res
end





--- Outputs the detection results
-- a_DetectionRes is a dict table of SongFileName -> {array of {mpm = .., confidence = ..}}
local function outputDetectionResults(a_DetectionRes)
	print("{")
	for songFileName, info in pairs(a_DetectionRes) do
		print("\t[\"" .. songFileName .. "\"] =")
		print("\t{")
		for _, cg in ipairs(info.confidences or {}) do
			print("\t\t{ mpm = " .. (cg[1] or "-1") .. ", confidence = " .. (cg[2] or "-1") .. "},")
		end
		print("\t},")
	end
	print("}")
end





local g_MatchingTempoFactors = {1, 2, 3, 4, 6, 8, 12, 16}

--- Returns true iff the two tempos are compatible (up to a factor)
local function isCompatibleTempo(a_Tempo1, a_Tempo2)
	for _, m in ipairs(g_MatchingTempoFactors) do
		if (
			(math.abs(a_Tempo1 - a_Tempo2 * m) < m) or
			(math.abs(a_Tempo1 * m - a_Tempo2) < m)
		) then
			return true
		end
	end
	return false
end





--- Calculates statistics over the detection results
-- a_DetectionRes is a dict table of SongFileName -> { <BeatDetectCmd results> }
local function calcStatistics(a_DetectionRes)
	local minMatchConfidenceRel = 1e16
	local maxWrongConfidenceRel = 0
	local numMatches = 0
	local numWrongs = 0
	for songFileName, info in pairs(a_DetectionRes) do
		local storedMPM = (info.parsedID3Tag or {}).mpm
		if (storedMPM and info.confidences and info.confidences[1] and info.confidences[2]) then
			print(songFileName)
			print("  Stored: " .. storedMPM)
			local detectedMpm = info.confidences[1][1]
			print("  Detected: " .. tostring(detectedMpm))
			local confidenceRel = info.confidences[1][2] / info.confidences[2][2]
			if (isCompatibleTempo(detectedMpm, storedMPM)) then
				print("  Tempo MATCH, confidence rel = " .. confidenceRel)
				if (minMatchConfidenceRel > confidenceRel) then
					minMatchConfidenceRel = confidenceRel
				end
				numMatches = numMatches + 1
			else
				print("  Tempo WRONG, confidence rel = " .. confidenceRel)
				if (maxWrongConfidenceRel < confidenceRel) then
					maxWrongConfidenceRel = confidenceRel
				end
				numWrongs = numWrongs + 1
			end
		end
	end
	print("---")
	print("Matches: " .. numMatches)
	print("Wrong: " .. numWrongs)
	print("minMatchConfidenceRel = " .. minMatchConfidenceRel)
	print("maxWrongConfidenceRel = " .. maxWrongConfidenceRel)
end





local detected = processParams({...})
outputDetectionResults(detected)
calcStatistics(detected)
