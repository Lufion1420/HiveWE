#pragma once

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QElapsedTimer>
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
/// thumbnail therefore leaves no mesh and no new texture resident. A model that fails to load or draw gets a placeholder and is not retried (parse failures and StillOnly are permanent; an animated tile or atlas that fails is only skipped until the next stop_animation()).
///
/// The offscreen context is one per process (see ensure_renderer), shared by every cache. On top of that a few tiles can play their animation (animate()): their models stay loaded in the same private context, all of them are
/// drawn into one atlas framebuffer per tick (animated_tile_fps), read back once and handed out as pixmaps. They are never written to the
/// memory or disk cache: the still thumbnail stays the fallback. Their textures are evicted again when they stop.
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

	/// Animated tiles (see TileAnimation): `paths` play their preview sequence, everything else goes back to its still thumbnail. Models
	/// already playing keep playing; new ones are parsed on workers once the still thumbnails are done and set up a couple per tick. A
	/// model without a usable sequence, or whose still thumbnail failed, stays still. Does nothing when animate_browse_tiles is false.
	void animate(const QStringList& paths);
	/// Stops drawing frames; the tiles keep their last frame and their models until animate() or stop_animation().
	void pause_animation();
	/// Stops drawing and releases every animated model (mesh, textures); the tiles show their still thumbnail again.
	void stop_animation();
	/// The latest animated frame of `path`, or a null pixmap when it is not playing.
	QPixmap animated_pixmap(const QString& path) const;
	int animated_tiles() const;
	int animation_frames() const { return animation_frame_count; }

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
	/// A new animated frame for `path`, or `path` stopped playing and shows its still thumbnail again.
	void animation_frame(const QString& path);

  private:
	struct Job;
	struct Ready;
	class Renderer;

	void tick();
	void animation_tick();
	/// The one offscreen context of the process, shared by every cache (each dialog used to create its own, and every new context made
	/// the driver grow its private memory again once tiles were drawn). Created on first use, deleted at aboutToQuit. Tiles are tagged
	/// with the cache that owns them.
	static Renderer& ensure_renderer();
	/// Null before the first use and after aboutToQuit.
	static Renderer* live_renderer();
	static Renderer* shared_renderer; // deleted at aboutToQuit, while the GL platform still exists
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

	// Animated tiles. The models themselves live in the Renderer (their GL objects belong to its context).
	QTimer* animation_timer;
	QElapsedTimer animation_clock;
	std::vector<Queued> animation_wanted; // in priority order
	std::vector<std::unique_ptr<Job>> animation_jobs;
	std::unordered_map<std::string, QPixmap> animation_frames_by_norm;
	std::unordered_set<std::string> still_only; // normalised paths that have nothing to play
	std::unordered_set<std::string> temporarily_failed; // tile or atlas failures; cleared by stop_animation so a later start retries
	int animation_frame_count = 0;

	int generation = 0;
	int generated_count = 0;
	int disk_count = 0;
	int evicted_count = 0;
};
