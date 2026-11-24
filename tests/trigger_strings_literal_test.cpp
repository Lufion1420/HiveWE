#include <doctest/doctest.h>

import std;
import TriggerStrings;

// w3i name/author/description may hold a literal string instead of a TRIGSTR_ key (the field is stored raw in the w3i,
// so an untouched literal round-trips by itself). The Map Description dialog reads with string() and writes with set_string().
// An edited literal must become a TRIGSTR_ entry: the generated script pastes literals into code.

TEST_CASE("a literal w3i string is shown as is by the dialog getter") {
	TriggerStrings ts;
	CHECK(ts.string("My Map") == "My Map");
	CHECK(ts.string("") == "");
	CHECK(ts.string("TRIGSTR_001") == ""); // unknown reference stays empty
}

TEST_CASE("editing a literal moves the text to a TRIGSTR_ entry") {
	TriggerStrings ts;
	std::string name = "My Map";
	ts.set_string(name, "Edited Map");
	CHECK(name.rfind("TRIGSTR_", 0) == 0);
	CHECK(ts.string(name) == "Edited Map");
}

TEST_CASE("a multiline description with quotes and backslashes edited on a literal ends as a TRIGSTR_ key") {
	TriggerStrings ts;
	std::string description = "old";
	const std::string text = "Line one \"quoted\"\nC:\\path\\to\nLine three";
	ts.set_string(description, text);
	CHECK(description.rfind("TRIGSTR_", 0) == 0);
	CHECK(description.find('"') == std::string::npos);
	CHECK(description.find('\n') == std::string::npos);
	CHECK(description.find('\\') == std::string::npos);
	CHECK(ts.string(description) == text);
}

TEST_CASE("writing back the unchanged literal leaves it untouched") {
	TriggerStrings ts;
	std::string description = "A\nmultiline description";
	const std::string shown(ts.string(description));
	ts.set_string(description, shown);
	CHECK(description == "A\nmultiline description");
}

TEST_CASE("clearing a literal leaves it empty") {
	TriggerStrings ts;
	std::string author = "Someone";
	ts.set_string(author, "");
	CHECK(author.empty());
	CHECK(ts.string(author) == "");
}

TEST_CASE("empty key still creates a TRIGSTR reference, existing references still update") {
	TriggerStrings ts;
	std::string key;
	ts.set_string(key, "Hello");
	CHECK(key == "TRIGSTR_001");
	CHECK(ts.string(key) == "Hello");
	ts.set_string(key, "Changed");
	CHECK(key == "TRIGSTR_001");
	CHECK(ts.string(key) == "Changed");

	std::string empty;
	ts.set_string(empty, "");
	CHECK(empty.empty());

	std::string second;
	ts.set_string(second, "Other");
	CHECK(second == "TRIGSTR_002");
}
