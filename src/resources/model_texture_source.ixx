module;

// #include instead of `import std`: Qt .cpp files that include std headers after their imports see this module (like Texture).
#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

export module ModelTextureSource;

namespace fs = std::filesystem;

/// A texture one model's own source provides, used instead of the Hierarchy.
export struct ModelTexture {
	/// ResourceManager identity. It starts with a NUL, which no texture path holds, so it never meets the key of a map or game texture.
	/// It names the file and its size and write time, so an edited file is read again.
	std::string cache_key;
	fs::path file;
};

/// Where one open model looks for its textures before the Hierarchy. Every model carries its own; the default one finds nothing, which is
/// right for map and game models. A later .wmpack reader belongs in find(), ahead of the folder.
export class ModelTextureSource {
	fs::path folder;

	/// The name the model stores, if it can only point inside the folder: relative, no drive or stream, and, after an optional leading
	/// "." part, no component that is empty (rooted, UNC, doubled separator) or ends in a dot or space (".", ".." and the spellings
	/// Windows trims into them). The name comes from the model file, so it is untrusted.
	static std::optional<fs::path> contained_name(const fs::path& referenced) {
		std::wstring name;
		try {
			// The model reader widened the stored bytes with the system code page. Model text is UTF-8, so read them as that if they are.
			const std::string bytes = referenced.string();
			name = fs::path(std::u8string(bytes.begin(), bytes.end())).wstring();
		} catch (const std::system_error&) {
			name = referenced.wstring();
		}

		// One leading "." part only names the folder itself.
		if (name.starts_with(L".\\") || name.starts_with(L"./")) {
			name.erase(0, 2);
		}
		if (name.find_first_of(std::wstring_view(L":\0", 2)) != std::wstring::npos) {
			return std::nullopt;
		}
		for (std::size_t start = 0; start <= name.size();) {
			const std::size_t end = std::min(name.find_first_of(L"/\\", start), name.size());
			if (end == start || name[end - 1] == L'.' || name[end - 1] == L' ') {
				return std::nullopt;
			}
			start = end + 1;
		}
		return fs::path(name);
	}

  public:
	ModelTextureSource() = default;

	/// For a model opened from disk: the files in its own folder.
	static ModelTextureSource beside(const fs::path& model_file) {
		ModelTextureSource source;
		std::error_code ec;
		const fs::path absolute = fs::absolute(model_file, ec);
		if (!ec) {
			source.folder = absolute.parent_path();
		}
		return source;
	}

	/// `referenced` is the texture name as the model stores it. Only exactly that file counts (no other extension is tried).
	std::optional<ModelTexture> find(const fs::path& referenced) const {
		if (folder.empty()) {
			return std::nullopt;
		}
		const auto name = contained_name(referenced);
		if (!name) {
			return std::nullopt;
		}

		const fs::path file = folder / *name;
		std::error_code ec;
		if (!fs::is_regular_file(file, ec)) {
			return std::nullopt;
		}
		const auto size = fs::file_size(file, ec);
		if (ec) {
			return std::nullopt;
		}
		const auto written = fs::last_write_time(file, ec);
		if (ec) {
			return std::nullopt;
		}

		const std::u8string text = file.u8string();
		std::string key("\0model:", 7);
		key.append(text.begin(), text.end());
		key += "|" + std::to_string(size) + "|" + std::to_string(written.time_since_epoch().count()) + "|";
		return ModelTexture { .cache_key = std::move(key), .file = file };
	}
};
