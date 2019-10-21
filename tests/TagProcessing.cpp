// TagProcessing.cpp

// Tests the MetadataScanner's tag processing (FileTag to Song::Tag transformation)




#include <iostream>
#include "../src/MetadataScanner.hpp"
#include "../src/BackgroundTasks.hpp"




/** Global failure flag, any failing test sets this to true.
The program's exit status is set according to this value. */
static bool g_HasFailed = false;





/** Outputs the specified optional value to the stream, saying "(none)" if the value is not present. */
static std::ostream & operator <<(std::ostream & aStream, const DatedOptional<QString> & aValue)
{
	if (!aValue.isPresent())
	{
		aStream << "(none)";
	}
	else
	{
		aStream << "\"" << aValue.value().toStdString() << "\"";
	}
	return aStream;
}





/** Outputs the specified optional value to the stream, saying "(none)" if the value is not present. */
static std::ostream & operator <<(std::ostream & aStream, const DatedOptional<double> & aValue)
{
	if (!aValue.isPresent())
	{
		aStream << "(none)";
	}
	else
	{
		aStream << aValue.value();
	}
	return aStream;
}





/** Returns true if the two optional strings are equal.
Note that equality is rather loose here - an empty DatedOptional matches an empty string. */
static bool areEqual(const DatedOptional<QString> & aValue1, const DatedOptional<QString> & aValue2)
{
	if (aValue1.isEmpty() && aValue2.isEmpty())
	{
		return true;
	}
	return (aValue1.valueOrDefault() == aValue2.valueOrDefault());
}





/** Returns true if the two tags are equal.
Note that equality is specific here - an empty DatedOptional matches -1, to simplify writing test values. */
static bool areEqual(const DatedOptional<double> & aValue1, const DatedOptional<double> & aValue2)
{
	return (std::abs(aValue1.valueOr(-1) - aValue2.valueOr(-1)) < 0.000001);
}





/** Returns true if the two tags are equal.
Note that equality is rather loose here - an empty DatedOptional matches an empty string, etc. */
static bool areEqual(const Song::Tag & aTag1, const Song::Tag & aTag2)
{
	return (
		areEqual(aTag1.mAuthor,            aTag2.mAuthor) &&
		areEqual(aTag1.mTitle,             aTag2.mTitle) &&
		areEqual(aTag1.mGenre,             aTag2.mGenre) &&
		areEqual(aTag1.mMeasuresPerMinute, aTag2.mMeasuresPerMinute)
	);
}





/** Prints out the result of the comparison of the specified value.
a_DataName is the user-visible name of the data being compared (such as "author", "title" etc.) */
template <typename T>
static void printComparison(
	const char * aDataName,
	const DatedOptional<T> & aParsedValue,
	const DatedOptional<T> & aExpectedValue
)
{
	if (areEqual(aParsedValue, aExpectedValue))
	{
		std::cerr << "  Matched " << aDataName << ": " << aParsedValue << std::endl;
	}
	else
	{
		std::cerr << "  FAILED " << aDataName << std::endl;
		std::cerr << "    Expected: " << aExpectedValue << std::endl;
		std::cerr << "    Parsed:   " << aParsedValue   << std::endl;
	}
}





static void testTagParsing(const MetadataScanner::Tag & aInputTag, const Song::Tag & aExpectedOutput)
{
	auto parsed = MetadataScanner::parseId3Tag(aInputTag);
	if (areEqual(parsed, aExpectedOutput))
	{
		return;
	}

	// Output the comparison:
	std::cerr << "Tag parsing failed:" << std::endl;
	std::cerr << "  Input author:  " << aInputTag.mAuthor << std::endl;
	std::cerr << "  Input title:   " << aInputTag.mTitle << std::endl;
	std::cerr << "  Input comment: " << aInputTag.mComment<< std::endl;
	std::cerr << "  Input genre:   " << aInputTag.mGenre << std::endl;
	std::cerr << "  Input BPM:     " << aInputTag.mMeasuresPerMinute << "\"" << std::endl;
	printComparison("author", parsed.mAuthor,            aExpectedOutput.mAuthor);
	printComparison("title ", parsed.mTitle,             aExpectedOutput.mTitle);
	printComparison("genre ", parsed.mGenre,             aExpectedOutput.mGenre);
	printComparison("MPM   ", parsed.mMeasuresPerMinute, aExpectedOutput.mMeasuresPerMinute);
	g_HasFailed = true;

	// Re-parse (so that a debugger breakpoint can be put here to observe while stepping through):
	MetadataScanner::parseId3Tag(aInputTag);
}





static void testFileNameParsing(const QString & aFileName, const Song::Tag & aExpectedOutput)
{
	auto parsed = MetadataScanner::parseFileNameIntoMetadata(aFileName);
	if (parsed == aExpectedOutput)
	{
		return;
	}

	// Output the comparison:
	std::cerr << "FileName parsing failed:" << std::endl;
	std::cerr << "  Input FileName:  \"" << aFileName.toStdString() << "\"" << std::endl;
	printComparison("author", parsed.mAuthor,            aExpectedOutput.mAuthor);
	printComparison("title ", parsed.mTitle,             aExpectedOutput.mTitle);
	printComparison("genre ", parsed.mGenre,             aExpectedOutput.mGenre);
	printComparison("MPM   ", parsed.mMeasuresPerMinute, aExpectedOutput.mMeasuresPerMinute);
	g_HasFailed = true;

	// Re-parse (so that a debugger breakpoint can be put here to observe while stepping through):
	MetadataScanner::parseFileNameIntoMetadata(aFileName);
}





int main()
{
	static std::pair<MetadataScanner::Tag, Song::Tag> tagTests[] =
	{
		{{"author",          "title", "comment", "slowfox", 30}, {"author", "title", "SF", 30}},
		{{"-WALTZ   29 BPM", "title", "",        ""},            {"",       "title", "SW", 29}},
		{{"-a   29 BPM",     "title", "",        ""},            {"a",      "title", "",   29}},
		{{"tango(32)", "Tango mascarada", "",    ""},            {"", "Tango mascarada", "TG", 32}},
		{{"Pedro Gonez - Tango Misterioso", "Tango 32 Bpm", "", ""}, {"Pedro Gonez", "Tango Misterioso", "TG", 32}},
		{{"Various", "TG 32tpm - Claudius Alzner - Hernando's Hideaway", "", "Tango"}, {"Claudius Alzner", "Hernando's Hideaway", "TG", 32}},
		{{"Johnny Mathis", "Darling Lily - Slowfox - 29bpm", "", ""}, {"Johnny Mathis", "Darling Lily", "SF", 29}},
		{{"", "Casa Musica / TG-32-La Viguela", "", "Tango"}, {"Casa Musica", "La Viguela", "TG", 32}},
		{{"SW 29 Mike Oldfield", "Daydream", "", "Slow Waltz"}, {"Mike Oldfield", "Daydream", "SW", 29}},
	};
	for (const auto & test: tagTests)
	{
		testTagParsing(test.first, test.second);
	}

	static std::pair<QString, Song::Tag> fileNameTests[] =
	{
		{"waltz/[30 MPM] author - title.mp3",     {"author", "title", "SW", 30}},
		{"album/author - title (sb50).mp3",       {"author", "title", "SB", 50}},
		{"album/author - title (slowfox 29).mp3", {"author", "title", "SF", 29}},
		{"valcik/author - title waltz 60.mp3",    {"author", "title waltz 60", "VW"}},  // A single separate number has no real meaning (track? mpm? index?)
		{"album/01 unknown1.mp3",                 {"", "", ""}},
		{"QS Little Talks-Dj Ice & Monsters.mp3", {"Little Talks", "Dj Ice & Monsters", "QS"}},
	};
	for (const auto & test: fileNameTests)
	{
		testFileNameParsing(test.first, test.second);
	}

	if (!g_HasFailed)
	{
		std::cerr << "All tests passed" << std::endl;
	}
	return g_HasFailed ? 1 : 0;
}




// Dummy functions needed to compile the used parts of the sources:
void BackgroundTasks::enqueue(const QString &, std::function<void()>, bool, std::function<void()>)
{
}
