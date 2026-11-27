#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

import std;
import ModelTextureSource;
import ResourceManager;

namespace fs = std::filesystem;

// Asana 1220963622495886: a model opened from disk takes its textures from its own folder.

namespace {
	/// A scratch tree: <root>/model is the model's folder, <root> itself is "outside" it.
	struct ScratchModelFolder {
		fs::path root;
		fs::path folder;

		explicit ScratchModelFolder(const std::wstring& name) {
			root = fs::temp_directory_path() / "hivewe_model_texture_source_test" / name;
			std::error_code ec;
			fs::remove_all(root, ec);
			folder = root / "model";
			fs::create_directories(folder);
		}
		~ScratchModelFolder() {
			std::error_code ec;
			fs::remove_all(root, ec);
		}
		ScratchModelFolder(const ScratchModelFolder&) = delete;
		ScratchModelFolder& operator=(const ScratchModelFolder&) = delete;

		ModelTextureSource source() const {
			return ModelTextureSource::beside(folder / "effect.mdx");
		}
	};

	void write_text(const fs::path& path, const std::string& text) {
		fs::create_directories(path.parent_path());
		std::ofstream out(path, std::ios::binary);
		out << text;
	}

	std::string utf8(const char8_t* text) {
		return reinterpret_cast<const char*>(text);
	}

	struct Named : Resource {
		static constexpr const char* name = "Named";
		std::string text;
		explicit Named(const fs::path& path) : text(path.string()) {}
		Named(const std::string& text, int) : text(text) {}
	};
}

TEST_CASE("model texture source: exactly the referenced file in the model's folder") {
	const ScratchModelFolder scratch(L"exact");
	write_text(scratch.folder / "Glow.blp", "glow");
	write_text(scratch.folder / "only.tga", "tga");
	write_text(scratch.folder / "Textures" / "Nested.blp", "nested");
	fs::create_directories(scratch.folder / "folder.blp");
	const ModelTextureSource source = scratch.source();

	const auto glow = source.find("Glow.blp");
	REQUIRE(glow.has_value());
	CHECK(fs::equivalent(glow->file, scratch.folder / "Glow.blp"));

	// Case as Windows treats it, and both separators.
	CHECK(source.find("gLOW.BLP").has_value());
	CHECK(source.find("Textures\\Nested.blp").has_value());
	CHECK(source.find("textures/nested.blp").has_value());
	// A leading "." part names the folder itself.
	CHECK(source.find(".\\Glow.blp").has_value());
	CHECK(source.find("./Textures/Nested.blp").has_value());

	// No other extension is tried, a folder is no texture, and a missing name is left to the normal resolution.
	CHECK_FALSE(source.find("only.blp").has_value());
	CHECK_FALSE(source.find("only.dds").has_value());
	CHECK_FALSE(source.find("Glow").has_value());
	CHECK_FALSE(source.find("folder.blp").has_value());
	CHECK_FALSE(source.find("absent.blp").has_value());
	CHECK_FALSE(source.find("").has_value());
}

TEST_CASE("model texture source: a map or game model has none") {
	CHECK_FALSE(ModelTextureSource().find("Glow.blp").has_value());
	CHECK_FALSE(ModelTextureSource().find("Textures\\white.blp").has_value());
}

TEST_CASE("model texture source: never leaves the model's folder") {
	const ScratchModelFolder scratch(L"contained");
	write_text(scratch.root / "outside.blp", "outside");
	write_text(scratch.folder / "inside.blp", "inside");
	write_text(scratch.folder / "sub" / "deep.blp", "deep");
	const ModelTextureSource source = scratch.source();
	const std::string outside = (scratch.root / "outside.blp").string();

	// Every one of these names an existing file.
	const std::vector<std::string> names = {
		"..\\outside.blp",
		"../outside.blp",
		"sub/../../outside.blp",
		"sub\\..\\inside.blp",
		".. \\outside.blp",
		".",
		".\\",
		".\\..\\outside.blp",
		".\\\\inside.blp",
		".\\.\\inside.blp",
		"sub\\.\\deep.blp",
		outside,
		outside.substr(2), // rooted, no drive
		outside.substr(0, 2) + "outside.blp", // drive relative
		"\\\\?\\" + outside,
		"\\\\localhost\\" + outside.substr(0, 1) + "$" + outside.substr(2),
		"\\inside.blp",
		"/inside.blp",
		"sub\\\\deep.blp",
		"inside.blp::$DATA",
		"inside.blp.",
		std::string("inside.blp\0x", 12),
	};
	for (const std::string& name : names) {
		CAPTURE(name);
		CHECK_FALSE(source.find(fs::path(name)).has_value());
	}
}

TEST_CASE("model texture source: cache identity is per folder, per file version and apart from map and game textures") {
	const ScratchModelFolder first(L"identity_first");
	const ScratchModelFolder second(L"identity_second");
	write_text(first.folder / "same.blp", "one");
	write_text(second.folder / "same.blp", "two");

	const auto a = first.source().find("same.blp");
	const auto b = second.source().find("same.blp");
	REQUIRE(a.has_value());
	REQUIRE(b.has_value());
	CHECK(a->cache_key != b->cache_key);
	CHECK(a->cache_key == first.source().find("same.blp")->cache_key);
	// A texture path never holds a NUL, so no map or game key starts like this.
	CHECK(a->cache_key.front() == '\0');

	write_text(first.folder / "same.blp", "edited and longer");
	CHECK(first.source().find("same.blp")->cache_key != a->cache_key);
}

TEST_CASE("model texture source: non-ASCII folder and texture names") {
	// Cyrillic and CJK: no single system code page holds both.
	const ScratchModelFolder scratch(L"Эффект_火");
	write_text(scratch.folder / "Glow.blp", "glow");
	write_text(scratch.folder / fs::path(u8"flämme.blp"), "flame");
	const ModelTextureSource source = scratch.source();

	const auto glow = source.find("Glow.blp");
	REQUIRE(glow.has_value());
	CHECK(fs::equivalent(glow->file, scratch.folder / "Glow.blp"));

	// As the model reader builds the path: from the file's UTF-8 bytes.
	CHECK(source.find(fs::path(utf8(u8"flämme.blp"))).has_value());
}

TEST_CASE("model texture source: a name stored in the system code page, not UTF-8") {
	const ScratchModelFolder scratch(L"code_page");
	write_text(scratch.folder / fs::path(u8"flämme.blp"), "flame");
	// The raw CP1252 byte for a-umlaut, widened as the model reader does it. Only meaningful where that is the system code page.
	std::optional<fs::path> referenced;
	try {
		referenced = fs::path(std::string("fl\xE4mme.blp"));
	} catch (const std::system_error&) {
	}
	if (!referenced || referenced->wstring() != L"flämme.blp") {
		MESSAGE("skipped: the system code page is not CP1252-like");
		return;
	}
	const auto found = scratch.source().find(*referenced);
	REQUIRE(found.has_value());
	CHECK(fs::equivalent(found->file, scratch.folder / fs::path(u8"flämme.blp")));
}

TEST_CASE("resource manager: keyed resources are cached by their key alone") {
	ResourceManager manager;
	const auto by_path = manager.load<Named>("same.blp", "0").value();
	const auto first = manager.load_keyed<Named>(std::string("\0model:a/same.blp0", 18), std::string("a"), 0).value();
	const auto second = manager.load_keyed<Named>(std::string("\0model:b/same.blp0", 18), std::string("b"), 0).value();
	const auto again = manager.load_keyed<Named>(std::string("\0model:a/same.blp0", 18), std::string("unused"), 0).value();

	CHECK(first == again);
	CHECK(first != second);
	CHECK(first != by_path);
	CHECK(second->text == "b");
	CHECK(manager.load<Named>("same.blp", "0").value() == by_path);
}
