#include <doctest/doctest.h>

import std;
import Hierarchy;
import Imports;
import Triggers;
import ProtectionPipeline;

namespace fs = std::filesystem;

// Save failures must surface as errors and leave the map's existing files byte-identical (Asana 1220330960297388).

namespace {
	fs::path make_scratch_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_map_save_failure_test" / name;
		std::error_code ec;
		if (fs::exists(dir, ec)) {
			for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
				fs::permissions(entry.path(), fs::perms::owner_write, fs::perm_options::add, ec);
			}
		}
		fs::remove_all(dir, ec);
		fs::create_directories(dir, ec);
		return dir;
	}

	std::string read_all(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
	}

	void write_all(const fs::path& path, const std::string& content) {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << content;
	}

	void make_read_only(const fs::path& path) {
		fs::permissions(path, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove);
	}

	void make_writable(const fs::path& path) {
		std::error_code ec;
		fs::permissions(path, fs::perms::owner_write, fs::perm_options::add, ec);
	}

	std::span<const std::uint8_t> bytes(const std::string& text) {
		return { reinterpret_cast<const std::uint8_t*>(text.data()), text.size() };
	}

	/// Points hierarchy.map_directory at a scratch map folder for one test and restores it afterwards.
	struct MapDirectoryScope {
		fs::path original = hierarchy.map_directory;
		explicit MapDirectoryScope(const fs::path& directory) {
			hierarchy.map_directory = directory;
		}
		~MapDirectoryScope() {
			hierarchy.map_directory = original;
		}
	};

	bool contains(const std::string& haystack, const std::string_view needle) {
		return haystack.find(needle) != std::string::npos;
	}

	struct ScriptSandbox {
		fs::path root;
		fs::path map;
		fs::path tools;
		MapScriptOutput output;

		explicit ScriptSandbox(const std::string& name) : root(make_scratch_dir(name)), map(root / "map"), tools(root / "tools") {
			fs::create_directories(map);
			fs::create_directories(tools);
			output.input_script = root / "input.lua";
			output.tools_directory = tools;
			output.compiler = tools / "compiler.cmd";
			output.compile_timeout = std::chrono::seconds(30);
		}

		void write_compiler(const std::string& body) const {
			write_all(tools / "compiler.cmd", "@echo off\r\n" + body + "\r\n");
		}
	};
} // namespace

// F4: map_file_write goes through <path>.hivewe-tmp and a rename.

TEST_CASE("map_file_write replaces the target and leaves no temporary file") {
	const fs::path dir = make_scratch_dir("write_ok");
	const MapDirectoryScope scope(dir);
	write_all(dir / "war3map.w3e", "OLD CONTENT THAT IS LONGER");

	hierarchy.map_file_write("war3map.w3e", bytes("NEW"));

	CHECK(read_all(dir / "war3map.w3e") == "NEW");
	CHECK_FALSE(fs::exists(dir / "war3map.w3e.hivewe-tmp"));
}

TEST_CASE("map_file_write to a read-only target throws and keeps the old bytes") {
	const fs::path dir = make_scratch_dir("write_read_only");
	const MapDirectoryScope scope(dir);
	write_all(dir / "war3map.w3e", "OLD");
	make_read_only(dir / "war3map.w3e");

	CHECK_THROWS(hierarchy.map_file_write("war3map.w3e", bytes("NEW CONTENT")));

	make_writable(dir / "war3map.w3e");
	CHECK(read_all(dir / "war3map.w3e") == "OLD");
	CHECK_FALSE(fs::exists(dir / "war3map.w3e.hivewe-tmp"));
}

TEST_CASE("map_file_write onto a directory throws and leaves no temporary file") {
	const fs::path dir = make_scratch_dir("write_directory");
	const MapDirectoryScope scope(dir);
	fs::create_directories(dir / "war3map.w3e");

	CHECK_THROWS(hierarchy.map_file_write("war3map.w3e", bytes("NEW")));

	CHECK(fs::is_directory(dir / "war3map.w3e"));
	CHECK_FALSE(fs::exists(dir / "war3map.w3e.hivewe-tmp"));
}

TEST_CASE("map_file_write leaves an import named <file>.tmp alone") {
	const fs::path dir = make_scratch_dir("write_keeps_tmp_import");
	const MapDirectoryScope scope(dir);
	write_all(dir / "war3map.w3e.tmp", "USER IMPORT");

	hierarchy.map_file_write("war3map.w3e", bytes("NEW"));

	CHECK(read_all(dir / "war3map.w3e") == "NEW");
	CHECK(read_all(dir / "war3map.w3e.tmp") == "USER IMPORT");
}

TEST_CASE("a leftover map_file_write temporary file is not listed in war3map.imp") {
	const fs::path dir = make_scratch_dir("imports_skip_temp");
	const MapDirectoryScope scope(dir);
	fs::create_directories(dir / "war3mapImported");
	write_all(dir / "war3mapImported" / "model.mdx", "MDX");
	write_all(dir / "war3mapImported" / "model.mdx.hivewe-tmp", "LEFT BY A CRASH");
	write_all(dir / "war3map.w3e.hivewe-tmp", "LEFT BY A CRASH");

	Imports().save(dir);

	const std::string imp = read_all(dir / "war3map.imp");
	REQUIRE(imp.size() >= 8);
	std::uint32_t count = 0;
	std::memcpy(&count, imp.data() + 4, sizeof(count));
	CHECK(count == 1);
	CHECK(contains(imp, "model.mdx"));
	CHECK_FALSE(contains(imp, "hivewe-tmp"));
}

TEST_CASE("remove_map_write_temp_files deletes only leftover temporary files") {
	const fs::path dir = make_scratch_dir("remove_temp_files");
	fs::create_directories(dir / "sub");
	write_all(dir / "war3map.w3e", "KEEP");
	write_all(dir / "notes.tmp", "KEEP");
	write_all(dir / "war3map.w3e.hivewe-tmp", "DROP");
	write_all(dir / "sub" / "a.blp.hivewe-tmp", "DROP");

	remove_map_write_temp_files(dir);

	CHECK(fs::exists(dir / "war3map.w3e"));
	CHECK(fs::exists(dir / "notes.tmp"));
	CHECK_FALSE(fs::exists(dir / "war3map.w3e.hivewe-tmp"));
	CHECK_FALSE(fs::exists(dir / "sub" / "a.blp.hivewe-tmp"));
}

// F1: Lua script input file.

TEST_CASE("install_map_script copies the Lua script into the map") {
	const ScriptSandbox sandbox("lua_ok");
	const MapDirectoryScope scope(sandbox.map);
	write_all(sandbox.map / "war3map.lua", "OLD");

	const auto result = install_map_script("NEW SCRIPT", ScriptMode::lua, sandbox.output);

	REQUIRE(result.has_value());
	CHECK(read_all(sandbox.map / "war3map.lua") == "NEW SCRIPT");
}

TEST_CASE("install_map_script refuses a Lua input file it cannot write and keeps war3map.lua") {
	const ScriptSandbox sandbox("lua_read_only_input");
	const MapDirectoryScope scope(sandbox.map);
	write_all(sandbox.map / "war3map.lua", "OLD");
	// The previous save's script, possibly from another map, locked against writing.
	write_all(sandbox.output.input_script, "STALE SCRIPT OF ANOTHER MAP");
	make_read_only(sandbox.output.input_script);

	const auto result = install_map_script("NEW SCRIPT", ScriptMode::lua, sandbox.output);

	make_writable(sandbox.output.input_script);
	CHECK_FALSE(result.has_value());
	CHECK(read_all(sandbox.map / "war3map.lua") == "OLD");
}

TEST_CASE("install_map_script refuses a Lua input path that is a folder and keeps war3map.lua") {
	const ScriptSandbox sandbox("lua_input_folder");
	const MapDirectoryScope scope(sandbox.map);
	write_all(sandbox.map / "war3map.lua", "OLD");
	fs::create_directories(sandbox.output.input_script);

	std::expected<void, std::string> result;
	CHECK_NOTHROW(result = install_map_script("NEW SCRIPT", ScriptMode::lua, sandbox.output));

	CHECK_FALSE(result.has_value());
	CHECK(read_all(sandbox.map / "war3map.lua") == "OLD");
}

// F2: JASS compiler process. The stub compiler gets the same arguments as clijasshelper: --scriptonly common.j blizzard.j
// <input> war3map.j, run in tools_directory.

TEST_CASE("install_map_script copies the compiled war3map.j into the map") {
	const ScriptSandbox sandbox("jass_ok");
	const MapDirectoryScope scope(sandbox.map);
	write_all(sandbox.map / "war3map.j", "OLD");
	sandbox.write_compiler("copy /y \"%~4\" war3map.j >nul\r\nexit /b 0");

	const auto result = install_map_script("function main takes nothing returns nothing\nendfunction\n", ScriptMode::jass, sandbox.output);

	REQUIRE_MESSAGE(result.has_value(), result.error());
	CHECK(read_all(sandbox.map / "war3map.j") == "function main takes nothing returns nothing\nendfunction\n");
}

TEST_CASE("install_map_script refuses a failing compiler and does not copy a stale war3map.j") {
	const ScriptSandbox sandbox("jass_exit_1");
	const MapDirectoryScope scope(sandbox.map);
	write_all(sandbox.map / "war3map.j", "OLD");
	write_all(sandbox.tools / "war3map.j", "STALE OUTPUT OF AN EARLIER SAVE");
	sandbox.write_compiler("exit /b 1");

	std::expected<void, std::string> result;
	CHECK_NOTHROW(result = install_map_script("NEW", ScriptMode::jass, sandbox.output));

	CHECK_FALSE(result.has_value());
	CHECK(read_all(sandbox.map / "war3map.j") == "OLD");
	CHECK_FALSE(fs::exists(sandbox.tools / "war3map.j"));
}

TEST_CASE("install_map_script reports the compiler's compile error and keeps war3map.j") {
	const ScriptSandbox sandbox("jass_compile_error");
	const MapDirectoryScope scope(sandbox.map);
	write_all(sandbox.map / "war3map.j", "OLD");
	// clijasshelper's measured behaviour on a syntax error: this text on stdout, no war3map.j, exit code 1.
	sandbox.write_compiler("echo logs\\currentmapscript.j: Compile error.\r\necho Line 5: Syntax Error\r\nexit /b 1");

	const auto result = install_map_script("NEW", ScriptMode::jass, sandbox.output);

	REQUIRE_FALSE(result.has_value());
	CHECK(contains(result.error(), "Compile error"));
	CHECK(read_all(sandbox.map / "war3map.j") == "OLD");
}

TEST_CASE("install_map_script refuses a compiler that exits 0 without writing war3map.j") {
	const ScriptSandbox sandbox("jass_no_output");
	const MapDirectoryScope scope(sandbox.map);
	write_all(sandbox.map / "war3map.j", "OLD");
	sandbox.write_compiler("exit /b 0");

	std::expected<void, std::string> result;
	CHECK_NOTHROW(result = install_map_script("NEW", ScriptMode::jass, sandbox.output));

	CHECK_FALSE(result.has_value());
	CHECK(read_all(sandbox.map / "war3map.j") == "OLD");
}

TEST_CASE("install_map_script stops a compiler that runs past the timeout") {
	ScriptSandbox sandbox("jass_timeout");
	const MapDirectoryScope scope(sandbox.map);
	write_all(sandbox.map / "war3map.j", "OLD");
	// Writes war3map.j only after about 4 seconds, well after the 1 second timeout.
	sandbox.write_compiler("ping -n 5 127.0.0.1 >nul 2>&1\r\ncopy /y \"%~4\" war3map.j >nul\r\nexit /b 0");
	sandbox.output.compile_timeout = std::chrono::seconds(1);

	const auto start = std::chrono::steady_clock::now();
	std::expected<void, std::string> result;
	CHECK_NOTHROW(result = install_map_script("NEW", ScriptMode::jass, sandbox.output));
	const auto elapsed = std::chrono::steady_clock::now() - start;

	REQUIRE_FALSE(result.has_value());
	CHECK(contains(result.error(), "did not finish"));
	CHECK(elapsed < std::chrono::seconds(4));
	CHECK(read_all(sandbox.map / "war3map.j") == "OLD");
}

TEST_CASE("install_map_script refuses a compiler that does not exist") {
	ScriptSandbox sandbox("jass_missing_compiler");
	const MapDirectoryScope scope(sandbox.map);
	write_all(sandbox.map / "war3map.j", "OLD");
	sandbox.output.compiler = sandbox.tools / "missing.exe";

	std::expected<void, std::string> result;
	CHECK_NOTHROW(result = install_map_script("NEW", ScriptMode::jass, sandbox.output));

	CHECK_FALSE(result.has_value());
	CHECK(read_all(sandbox.map / "war3map.j") == "OLD");
}

// F3: the Map Protector's save must restore the redirected paths even when the save throws.

TEST_CASE("save_restoring_state restores and reports a save that throws from map_file_write") {
	const fs::path dir = make_scratch_dir("protector_throw");
	const fs::path original_directory = dir / "original";
	const fs::path temp_directory = dir / "temp";
	fs::create_directories(original_directory);
	fs::create_directories(temp_directory / "war3map.w3e"); // a folder where the file goes: map_file_write throws
	const MapDirectoryScope scope(original_directory);

	fs::path filesystem_path = original_directory;
	bool restored = false;
	SyncSaveResult result;
	CHECK_NOTHROW(
		result = save_restoring_state(
			[&]() -> SyncSaveResult {
				hierarchy.map_directory = temp_directory;
				filesystem_path = temp_directory;
				hierarchy.map_file_write("war3map.w3e", bytes("NEW"));
				return { true, "" };
			},
			[&]() {
				restored = true;
				hierarchy.map_directory = original_directory;
				filesystem_path = original_directory;
			}
		)
	);

	CHECK(restored);
	CHECK_FALSE(result.success);
	CHECK(contains(result.error, "war3map.w3e"));
	CHECK(hierarchy.map_directory == original_directory);
	CHECK(filesystem_path == original_directory);
}

TEST_CASE("save_restoring_state restores after a save that returns failure") {
	bool restored = false;
	const SyncSaveResult result = save_restoring_state(
		[]() -> SyncSaveResult { return { false, "refused" }; },
		[&]() { restored = true; }
	);

	CHECK(restored);
	CHECK_FALSE(result.success);
	CHECK(result.error == "refused");
}
