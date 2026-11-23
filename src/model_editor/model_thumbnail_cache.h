#pragma once

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QTimer>

/// Renders one still picture per model (stand pose, frame 0) for the model browser, once, and keeps it in memory (QPixmapCache) and
/// optionally on disk under the app cache folder.
///
/// The work is spread over event-loop ticks: MDX files are read and parsed on std::async workers (never touching GL), the render happens
/// on the main thread in a private offscreen context that shares with the global share context, a couple of models per tick. The mesh is
/// destroyed while that context is current. EditableMesh loads its textures through ResourceManager, which keeps them until clear(), so
/// after each draw the textures that this model added and nothing else uses are evicted again (`resources_evicted()` counts them); a
/// thumbnail therefore leaves no mesh and no new texture resident. A model that fails to load or draw gets a placeholder and is not retried.
class ModelThumbnailCache : public QObject {
	Q_OBJECT

  public:
	static constexpr int thumbnail_size = 128;
	/// Drawn at this size and scaled down, which smooths the edges.
	static constexpr int render_size = 256;
	static constexpr int max_parses_in_flight = 3;
	static constexpr int max_renders_per_tick = 2;
	static constexpr int max_disk_loads_per_tick = 24;

	explicit ModelThumbnailCache(bool use_disk_cache, QObject* parent = nullptr);
	~ModelThumbnailCache() override;

	/// The thumbnail for a model path as listed by the catalog, or a null pixmap while it is not (or no longer) in memory.
	QPixmap pixmap(const QString& path) const;
	bool failed(const QString& path) const;
	/// A plain tile for a model that is still being drawn, or one with an exclamation mark for a model that could not be drawn.
	static QPixmap placeholder(bool failed);

	/// Replaces the queue with `paths` (most important first). Models that are already done are skipped. Work already started finishes.
	void request(const QStringList& paths);
	/// Drops everything queued; in-flight parses are discarded when they finish.
	void cancel();

	int generated() const { return generated_count; }
	int loaded_from_disk() const { return disk_count; }
	int failures() const { return static_cast<int>(failed_paths.size()); }
	/// Cached textures released again after drawing (see the class comment).
	int resources_evicted() const { return evicted_count; }
	/// Models queued, being parsed or waiting to be drawn.
	int pending() const;
	/// Why the first failures happened (for the QA result file), at most a handful.
	const std::vector<std::string>& failure_log() const { return failure_messages; }

  signals:
	/// A thumbnail (or the failed state of a model) is available for `path`, in the spelling given to request().
	void thumbnail_ready(const QString& path);
	/// The queue ran empty.
	void idle();

  private:
	struct Job;
	struct Ready;
	class Renderer;

	void tick();
	void schedule(bool waiting_on_workers);
	void harvest_jobs();
	bool try_disk(const QString& path, const std::string& norm);
	void store(const QString& path, const std::string& norm, const QImage& image, bool write_to_disk);
	void mark_failed(const QString& path, const std::string& norm, const std::string& why);
	std::string file_key(const QString& path, const std::string& norm);

	bool use_disk_cache;
	QString disk_directory;
	std::string game_stamp;
	QTimer* timer;

	struct Queued {
		QString path;
		std::string norm;
	};
	std::deque<Queued> queue;
	std::vector<std::unique_ptr<Job>> in_flight;
	std::vector<Ready> ready; // parsed models waiting for their turn on the main thread, oldest first

	std::unordered_map<std::string, std::string> file_keys; // normalised path -> thumbnail file name
	std::unordered_set<std::string> done; // normalised paths drawn at least once (may have left the memory cache)
	std::unordered_set<std::string> failed_paths;
	std::vector<std::string> failure_messages;
	std::unique_ptr<Renderer> renderer;

	int generation = 0;
	int generated_count = 0;
	int disk_count = 0;
	int evicted_count = 0;
};
