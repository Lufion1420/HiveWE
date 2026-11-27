export module ResourceManager;

import std;
import UnorderedMap;

namespace fs = std::filesystem;

export class Resource {
  public:
	virtual ~Resource() = default;
};

export class ResourceManager {
	std::mutex mutex;
	hive::unordered_map<std::string, std::shared_future<std::expected<std::shared_ptr<Resource>, std::string>>> resources;

	template <typename T, typename... Args>
	std::expected<std::shared_ptr<T>, std::string> load_impl(const std::string& key, Args&&... args) {
		std::unique_lock lock(mutex);

		if (const auto it = resources.find(key); it != resources.end()) {
			const auto fut = it->second;
			lock.unlock();
			auto result = fut.get();
			if (result) {
				return std::dynamic_pointer_cast<T>(*result);
			}
			return std::unexpected(result.error());
		}

		// We are the first caller for this key. Insert a future so concurrent callers block
		std::promise<std::expected<std::shared_ptr<Resource>, std::string>> promise;
		auto fut = promise.get_future().share();
		resources.emplace(key, fut);
		lock.unlock(); // Release before constructing. Constructor may call load() recursively

		try {
			auto res = std::make_shared<T>(std::forward<Args>(args)...);
			promise.set_value(res);
			return std::dynamic_pointer_cast<T>(res);
		} catch (const std::exception& e) {
			auto err = std::unexpected(std::string(e.what()));
			promise.set_value(err);
			return err;
		} catch (...) {
			auto err = std::unexpected(std::string("Unknown error loading resource"));
			promise.set_value(err);
			return err;
		}
	}

  public:
	/// Loads and caches a resource until clear() is called.
	/// Thread-safe: concurrent loads of the same key construct the resource exactly once.
	template <typename T, typename... Args>
	std::expected<std::shared_ptr<T>, std::string> load(const fs::path& path, const std::string& custom_identifier = "", Args... args) {
		static_assert(std::is_base_of_v<Resource, T>, "T must inherit from Resource");
		const std::string key = path.string() + T::name + custom_identifier;
		return load_impl<T>(key, path, std::forward<Args>(args)...);
	}

	template <typename T>
	std::expected<std::shared_ptr<T>, std::string> load(const std::initializer_list<fs::path> paths) {
		static_assert(std::is_base_of_v<Resource, T>, "T must inherit from Resource");

		std::string key;
		for (const auto& path : paths) {
			key += path.string();
		}
		key += T::name;

		return load_impl<T>(key, paths);
	}

	/// Like load, for a resource whose identity is not its path text (a model's own texture file, whose path may not fit the system code
	/// page). `args` go to the constructor.
	template <typename T, typename... Args>
	std::expected<std::shared_ptr<T>, std::string> load_keyed(const std::string& key, Args&&... args) {
		static_assert(std::is_base_of_v<Resource, T>, "T must inherit from Resource");
		return load_impl<T>(key + T::name, std::forward<Args>(args)...);
	}

	/// The keys cached right now. Pair with evict_unused_since.
	std::unordered_set<std::string> snapshot_keys() {
		std::lock_guard lock(mutex);
		std::unordered_set<std::string> keys;
		for (const auto& [key, future] : resources) {
			keys.insert(key);
		}
		return keys;
	}

	/// Drops the finished entries that are not in `before` and that nobody but the cache still holds. Everything that was cached before,
	/// or is still in use, stays. Returns how many were dropped. The dropped resources are destroyed here, on the calling thread, so a GPU
	/// resource needs the caller's GL context to be current.
	std::size_t evict_unused_since(const std::unordered_set<std::string>& before) {
		std::vector<std::shared_ptr<Resource>> dropped;
		{
			std::lock_guard lock(mutex);
			std::vector<std::string> erase_keys; // not erased while iterating: the map may move entries on erase
			for (const auto& [key, future] : resources) {
				if (before.contains(key) || future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
					continue;
				}
				const auto& result = future.get();
				if (!result) {
					erase_keys.push_back(key); // a failed load holds nothing
				} else if (result->use_count() == 1) {
					dropped.push_back(*result);
					erase_keys.push_back(key);
				}
			}
			for (const std::string& key : erase_keys) {
				resources.erase(key);
			}
		}
		const std::size_t count = dropped.size();
		dropped.clear();
		return count;
	}

	/// Drops the finished entries among `keys` that nobody but the cache still holds, and removes from `keys` every key that is no
	/// longer cached afterwards (dropped here or by someone else). Keys outside `keys` are never touched. Returns how many were dropped.
	/// Destroys on the calling thread, like evict_unused_since.
	std::size_t evict_unused_in(std::unordered_set<std::string>& keys) {
		std::vector<std::shared_ptr<Resource>> dropped;
		{
			std::lock_guard lock(mutex);
			std::vector<std::string> erase_keys;
			for (auto it = keys.begin(); it != keys.end();) {
				const auto found = resources.find(*it);
				if (found == resources.end()) {
					it = keys.erase(it);
					continue;
				}
				const auto& future = found->second;
				if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
					const auto& result = future.get();
					if (!result || result->use_count() == 1) {
						if (result) {
							dropped.push_back(*result);
						}
						erase_keys.push_back(*it);
						it = keys.erase(it);
						continue;
					}
				}
				++it;
			}
			for (const std::string& key : erase_keys) {
				resources.erase(key);
			}
		}
		const std::size_t count = dropped.size();
		dropped.clear();
		return count;
	}

	/// Releases all cached resources. Call when unloading a map.
	void clear() {
		std::lock_guard lock(mutex);
		resources.clear();
	}
};

export inline ResourceManager resource_manager;
