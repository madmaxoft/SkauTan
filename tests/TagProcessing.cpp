// TagProcessing.cpp

// Tests the MetadataScanner's tag processing (FileTag to Song::Tag transformation)




#include <iostream>
#include "../src/MetadataScanner.hpp"
#include "../src/BackgroundTasks.hpp"




/** Global failure flag, any failing test sets this to true.
The program's exit status is set according to this value. */
static bool g_HasFailed = false;





/** Outputs the specified optional value to the stream, saying "(none)" if the value is not present. */
static std::ostream & operator <<(std::ostream & a_Stream, const DatedOptional<QString> & a_Value)
{
	if (!a_Value.isPresent())
	{
		a_Stream << "(none)";
	}
	else
	{
		a_Stream << "\"" << a_Value.value().toStdString() << "\"";
	}
	return a_Stream;
}





/** Outputs the specified optional value to the stream, saying "(none)" if the value is not present. */
static std::ostream & operator <<(std::ostream & a_Stream, const DatedOptional<double> & a_Value)
{
	if (!a_Value.isPresent())
	{
		a_Stream << "(none)";
	}
	else
	{
		a_Stream << a_Value.value();
	}
	return a_Stream;
}





/** Returns true if the two optional strings are equal.
Note that equality is rather loose here - an empty DatedOptional matches an empty string. */
static bool areEqual(const DatedOptional<QString> & a_Value1, const DatedOptional<QString> & a_Value2)
{
	if (a_Value1.isEmpty() && a_Value2.isEmpty())
	{
		return true;
	}
	return (a_Value1.valueOrDefault() == a_Value2.valueOrDefault());
}





/** Returns true if the two tags are equal.
Note that equality is specific here - an empty DatedOptional matches -1, to simplify writing test values. */
static bool areEqual(const DatedOptional<double> & a_Value1, const DatedOptional<double> & a_Value2)
{
	return (std::abs(a_Value1.valueOr(-1) - a_Value2.valueOr(-1)) < 0.000001);
}





/** Returns true if the two tags are equal.
Note that equality is rather loose here - an empty DatedOptional matches an empty string, etc. */
static bool areEqual(const Song::Tag & a_Tag1, const Song::Tag & a_Tag2)
{
	return (
		areEqual(a_Tag1.m_Author,            a_Tag2.m_Author) &&
		areEqual(a_Tag1.m_Title,             a_Tag2.m_Title) &&
		areEqual(a_Tag1.m_Genre,             a_Tag2.m_Genre) &&
		areEqual(a_Tag1.m_MeasuresPerMinute, a_Tag2.m_MeasuresPerMinute)
	);
}





/** Prints out the result of the comparison of the specified value.
a_DataName is the user-visible name of the data being compared (such as "author", "title" etc.) */
template <typename T>
static void printComparison(
	const char * a_DataName,
	const DatedOptional<T> & a_ParsedValue,
	const DatedOptional<T> & a_ExpectedValue
)
{
	if (areEqual(a_ParsedValue, a_ExpectedValue))
	{
		std::cerr << "  Matched " << a_DataName << ": " << a_ParsedValue << std::endl;
	}
	else
	{
		std::cerr << "  FAILED " << a_DataName << std::endl;
		std::cerr << "    Expected: " << a_ExpectedValue << std::endl;
		std::cerr << "    Parsed:   " << a_ParsedValue   << std::endl;
	}
}





static void testTagParsing(const MetadataScanner::Tag & a_InputTag, const Song::Tag & a_ExpectedOutput)
{
	auto parsed = MetadataScanner::parseId3Tag(a_InputTag);
	if (areEqual(parsed, a_ExpectedOutput))
	{
		return;
	}

	// Output the comparison:
	std::cerr << "Tag parsing failed:" << std::endl;
	std::cerr << "  Input author:  " << a_InputTag.m_Author << std::endl;
	std::cerr << "  Input title:   " << a_InputTag.m_Title << std::endl;
	std::cerr << "  Input comment: " << a_InputTag.m_Comment<< std::endl;
	std::cerr << "  Input genre:   " << a_InputTag.m_Genre << std::endl;
	std::cerr << "  Input BPM:     " << a_InputTag.m_MeasuresPerMinute << "\"" << std::endl;
	printComparison("author", parsed.m_Author,            a_ExpectedOutput.m_Author);
	printComparison("title ", parsed.m_Title,             a_ExpectedOutput.m_Title);
	printComparison("genre ", parsed.m_Genre,             a_ExpectedOutput.m_Genre);
	printComparison("MPM   ", parsed.m_MeasuresPerMinute, a_ExpectedOutput.m_MeasuresPerMinute);
	g_HasFailed = true;

	// Re-parse (so that a debugger breakpoint can be put here to observe while stepping through):
	MetadataScanner::parseId3Tag(a_InputTag);
}





static void testFileNameParsing(const QString & a_FileName, const Song::Tag & a_ExpectedOutput)
{
	auto parsed = MetadataScanner::parseFileNameIntoMetadata(a_FileName);
	if (parsed == a_ExpectedOutput)
	{
		return;
	}

	// Output the comparison:
	std::cerr << "FileName parsing failed:" << std::endl;
	std::cerr << "  Input FileName:  \"" << a_FileName.toStdString() << "\"" << std::endl;
	printComparison("author", parsed.m_Author,            a_ExpectedOutput.m_Author);
	printComparison("title ", parsed.m_Title,             a_ExpectedOutput.m_Title);
	printComparison("genre ", parsed.m_Genre,             a_ExpectedOutput.m_Genre);
	printComparison("MPM   ", parsed.m_MeasuresPerMinute, a_ExpectedOutput.m_MeasuresPerMinute);
	g_HasFailed = true;

	// Re-parse (so that a debugger breakpoint can be put here to observe while stepping through):
	MetadataScanner::parseFileNameIntoMetadata(a_FileName);
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
