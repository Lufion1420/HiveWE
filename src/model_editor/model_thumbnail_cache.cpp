#include <glad/glad.h>

#include "model_thumbnail_cache.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QPixmapCache>
#include <QStandardPaths>
#include <QSurfaceFormat>

import std;
import BinaryReader;
import EditableMesh;
import Hierarchy;
import MDX;
import ModelCatalog;
import ResourceManager;
import Shader;
import SkeletalModelInstance;
import TileAnimation;
import TimelineMath;
import <glm/gtc/matrix_transform.hpp>;

namespace fs = std::filesystem;

namespace {
	constexpr float k_fov_deg = 50.f;
	constexpr float k_near = 0.05f;
	constexpr float k_far = 20000.f;

	QString memory_key(const std::string& file_key) {
		return QString("hwe-thumb:") + QString::fromStdString(file_key);
	}

	using ParseResult = std::pair<std::shared_ptr<mdx::MDX>, std::string>;

	/// Reads and parses a model on a worker. The worker never touches GL or Qt.
	std::future<ParseResult> parse_async(const QString& path) {
		const std::wstring name = path.toStdWString();
		return std::async(std::launch::async, [name]() -> ParseResult {
			try {
				auto reader = hierarchy.open_file(fs::path(name));
				if (!reader) {
					return { nullptr, reader.error() };
				}
				return { std::make_shared<mdx::MDX>(reader.value()), std::string() };
			} catch (const std::exception& e) {
				return { nullptr, e.what() };
			} catch (...) {
				return { nullptr, "unknown error while reading the model" };
			}
		});
	}

	struct Pose {
		glm::mat4 projection_view;
		glm::vec3 direction;
	};

	/// The camera of a thumbnail: looking at the extent of `sequence_index` (the model's extent when that one is empty) from the front left.
	Pose fit_pose(const mdx::MDX& mdx, const int sequence_index) {
		float fit_distance = 200.f;
		glm::vec3 fit_position(0.f);
		if (!mdx.sequences.empty() && sequence_index >= 0 && sequence_index < static_cast<int>(mdx.sequences.size())) {
			auto extent = mdx.sequences[sequence_index].extent;
			if (!(glm::length(extent.maximum - extent.minimum) > 0.001f)) {
				extent = mdx.extent;
			}
			const glm::vec3 size = extent.maximum - extent.minimum;
			const float radius = glm::length(size) * 0.5f;
			if (std::isfinite(radius) && radius > 0.001f) {
				fit_distance = radius / std::sin(glm::radians(k_fov_deg) * 0.5f);
				fit_position = glm::vec3(0.f, 0.f, extent.minimum.z + size.z * 0.5f);
			}
		}
		const glm::vec3 dir = glm::normalize(glm::vec3 { -1.f, 1.f, -0.5f });
		const glm::vec3 up = { 0, 0, 1 };
		const glm::vec3 eye = fit_position - dir * fit_distance;
		return { glm::perspective(glm::radians(k_fov_deg), 1.f, k_near, k_far) * glm::lookAt(eye, fit_position, up), dir };
	}

	/// The GL state both the still and the animated thumbnails draw with, and the clear to the tile colour.
	void begin_thumbnail_pass(const int width, const int height) {
		glViewport(0, 0, width, height);
		glDisable(GL_SCISSOR_TEST);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(true);
		glClearColor(0.20f, 0.22f, 0.25f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
} // namespace

struct ModelThumbnailCache::Job {
	QString path;
	std::string norm;
	int generation = 0;
	std::future<ParseResult> result;
};

struct ModelThumbnailCache::Ready {
	QString path;
	std::string norm;
	std::shared_ptr<mdx::MDX> mdx;
};

/// The private offscreen context and everything created in it: the still renders and the animated tiles. Created on first use, on the
/// main thread.
class ModelThumbnailCache::Renderer {
  public:
	Renderer() {
		surface.setFormat(QSurfaceFormat::defaultFormat());
		surface.create();
		context.setFormat(QSurfaceFormat::defaultFormat());
		context.setShareContext(QOpenGLContext::globalShareContext());
		usable = surface.isValid() && context.create();
	}

	~Renderer() {
		// The tile meshes and the atlases are GL objects of our context: destroy them with it current.
		if (usable && (!tiles.empty() || atlas || atlas_small || tile_vao)) {
			std::string error;
			if (make_current(error)) {
				std::size_t evicted = 0;
				clear_tiles(evicted);
				atlas.reset();
				atlas_small.reset();
				if (tile_vao) {
					glDeleteVertexArrays(1, &tile_vao);
				}
				release();
			} else {
				// ~EditableMesh calls glDeleteBuffers, which needs a current context: leak the meshes rather than call GL without one
				// (the textures stay owned by the resource cache, the atlases are freed by Qt's own shared-resource cleanup).
				for (Tile& tile : tiles) {
					(void)new std::shared_ptr<EditableMesh>(std::move(tile.mesh));
				}
				tiles.clear();
			}
		}
	}

	bool usable = false;

	/// Null image with `error` filled in when the model cannot be drawn.
	/// `evicted` is increased by the number of cached resources (the model's textures) released again after the draw.
	QImage render(const std::shared_ptr<mdx::MDX>& mdx, std::string& error, std::size_t& evicted) {
		if (!usable) {
			error = "no OpenGL context for thumbnails";
			return {};
		}
		if (!make_current(error)) {
			return {};
		}

		QImage image;
		GLuint vao = 0;
		std::unordered_set<std::string> cached_before;
		bool snapshot_taken = false;
		try {
			ensure_shaders();
			// EditableMesh loads its textures through ResourceManager, which keeps them until clear(). Whatever is added from here on
			// and not used by anything else is released again below, so a thumbnail leaves no texture resident.
			cached_before = resource_manager.snapshot_keys();
			snapshot_taken = true;

			QOpenGLFramebufferObjectFormat format;
			format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
			QOpenGLFramebufferObject fbo(ModelThumbnailCache::render_size, ModelThumbnailCache::render_size, format);
			if (!fbo.isValid()) {
				throw std::runtime_error("framebuffer is not valid");
			}

			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);
			fbo.bind();
			begin_thumbnail_pass(ModelThumbnailCache::render_size, ModelThumbnailCache::render_size);

			{
				// The mesh and skeleton die at the end of this block, while our context is still current.
				const auto mesh = std::make_shared<EditableMesh>(mdx, std::nullopt);
				SkeletalModelInstance skeleton(mdx);

				// Same pose rules as the Object Editor grid: "stand", else a sequence that has content.
				skeleton.set_sequence("stand");
				if (skeleton.sequence_index < 0
					|| (skeleton.sequence_index < static_cast<int>(mdx->sequences.size())
						&& SkeletalModelInstance::sequence_has_empty_extent(mdx->sequences[skeleton.sequence_index]))) {
					SkeletalModelInstance::pick_preview_sequence(skeleton, *mdx);
				}

				const Pose pose = fit_pose(*mdx, skeleton.sequence_index);

				skeleton.update_location(glm::vec3(0.f), glm::quat(), glm::vec3(1.f));
				// A node can come before its parent in the list, so one update leaves its matrix a step behind: update until it settles.
				for (int pass = 0; pass < 3; ++pass) {
					skeleton.update(0.0, false, false);
				}

				draw(*mesh, skeleton, pose);

				glFinish();
				image = fbo.toImage(); // flipped to top-down by default
			}

			glBindVertexArray(0);
			fbo.release();
		} catch (const std::exception& e) {
			error = e.what();
			image = QImage();
		} catch (...) {
			error = "unknown error while drawing";
			image = QImage();
		}

		if (vao) {
			glDeleteVertexArrays(1, &vao);
		}
		// The mesh is gone, so its textures are held by the cache alone. Destroyed here, with our context current.
		if (snapshot_taken) {
			evicted += resource_manager.evict_unused_since(cached_before);
		}
		release();

		if (image.isNull()) {
			if (error.empty()) {
				error = "nothing was drawn";
			}
			return {};
		}
		return image.convertToFormat(QImage::Format_RGB32)
			.scaled(ModelThumbnailCache::thumbnail_size, ModelThumbnailCache::thumbnail_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}

	// --- Animated tiles. Every call below makes the context current itself and leaves the caller's context as it was. -------------------

	enum class TileResult {
		Added,
		/// Nothing to play: no sequence with content and length.
		StillOnly,
		Failed
	};

	int tile_count(const void* owner) const {
		return static_cast<int>(std::ranges::count_if(tiles, [&](const Tile& tile) { return tile.owner == owner; }));
	}

	bool has_tile(const void* owner, const std::string& norm) const {
		return std::ranges::any_of(tiles, [&](const Tile& tile) { return tile.owner == owner && tile.norm == norm; });
	}

	/// Sets up a playing tile of `owner` for a parsed model.
	TileResult add_tile(
		const void* owner,
		const QString& path,
		const std::string& norm,
		const std::shared_ptr<mdx::MDX>& mdx,
		std::size_t& evicted
	) {
		std::vector<SequenceCandidate> candidates;
		candidates.reserve(mdx->sequences.size());
		for (const mdx::Sequence& sequence : mdx->sequences) {
			candidates.push_back({ sequence.name,
				sequence.end_frame > sequence.start_frame && !SkeletalModelInstance::sequence_has_empty_extent(sequence) });
		}
		const int sequence = preferred_preview_sequence(candidates);
		if (sequence < 0) {
			return TileResult::StillOnly;
		}
		if (!usable) {
			return TileResult::Failed;
		}
		std::string error;
		if (!make_current(error)) {
			return TileResult::Failed;
		}
		TileResult result = TileResult::Added;
		std::unordered_set<std::string> cached_before;
		try {
			ensure_shaders();
			// Only what this tile adds to the cache is released again later (see drop_tiles_except), never what the editor loaded
			// meanwhile.
			cached_before = resource_manager.snapshot_keys();
			Tile tile;
			tile.owner = owner;
			tile.path = path;
			tile.norm = norm;
			tile.mdx = mdx;
			tile.mesh = std::make_shared<EditableMesh>(mdx, std::nullopt);
			tile.skeleton = std::make_unique<SkeletalModelInstance>(mdx);
			tile.skeleton->set_sequence(sequence);
			tile.skeleton->update_location(glm::vec3(0.f), glm::quat(), glm::vec3(1.f));
			for (int pass = 0; pass < 3; ++pass) {
				tile.skeleton->update(0.0, false, false);
			}
			tile.pose = fit_pose(*mdx, sequence);
			tile.start = mdx->sequences[sequence].start_frame;
			tile.end = mdx->sequences[sequence].end_frame;
			tile.frame = tile.start;
			tiles.push_back(std::move(tile));
		} catch (...) {
			result = TileResult::Failed;
		}
		for (const std::string& key : resource_manager.snapshot_keys()) {
			if (!cached_before.contains(key)) {
				tile_keys.insert(key);
			}
		}
		if (result == TileResult::Failed) {
			evicted += resource_manager.evict_unused_in(tile_keys); // what the failed mesh loaded
		}
		release();
		return result;
	}

	/// Releases the tiles of `owner` not in `keep`, then the textures nothing uses any more. Returns the paths that stopped playing.
	std::vector<QString> drop_tiles_except(const void* owner, const std::unordered_set<std::string>& keep, std::size_t& evicted) {
		std::vector<QString> dropped;
		const auto goes = [&](const Tile& tile) { return tile.owner == owner && !keep.contains(tile.norm); };
		if (std::ranges::none_of(tiles, goes)) {
			return dropped;
		}
		std::string error;
		if (!make_current(error)) {
			// ~EditableMesh needs a current context: leak this owner's meshes rather than keep them (same fallback as ~Renderer).
			// tile_keys is not cleared here, consistent with ~Renderer: the textures stay cached until a later clear_tiles.
			for (auto it = tiles.begin(); it != tiles.end();) {
				if (!goes(*it)) {
					++it;
					continue;
				}
				dropped.push_back(it->path);
				(void)new std::shared_ptr<EditableMesh>(std::move(it->mesh));
				it = tiles.erase(it);
			}
			return dropped;
		}
		for (auto it = tiles.begin(); it != tiles.end();) {
			if (!goes(*it)) {
				++it;
				continue;
			}
			dropped.push_back(it->path);
			it = tiles.erase(it);
		}
		if (tiles.empty()) {
			clear_tiles(evicted); // also frees the atlases
		} else {
			evicted += resource_manager.evict_unused_in(tile_keys);
		}
		release();
		return dropped;
	}

	struct TileFrame {
		QString path;
		std::string norm;
		QImage image; // null when the tile failed to draw and was dropped
	};

	/// Moves every tile of `owner` on by `delta_seconds` and draws them all into one atlas, which is read back once.
	std::vector<TileFrame> render_tiles(const void* owner, const double delta_seconds, std::size_t& evicted) {
		std::vector<TileFrame> frames;
		std::vector<Tile*> mine;
		for (Tile& tile : tiles) {
			if (tile.owner == owner && mine.size() < max_animated_tiles) {
				mine.push_back(&tile);
			}
		}
		if (mine.empty() || !usable) {
			return frames;
		}
		std::string error;
		if (!make_current(error)) {
			return frames;
		}

		constexpr int big = ModelThumbnailCache::render_size;
		constexpr int small = ModelThumbnailCache::thumbnail_size;
		const int count = static_cast<int>(mine.size());
		QImage atlas_image;
		std::vector<bool> failed(mine.size(), false);
		try {
			ensure_shaders();
			if (!atlas) {
				// Drawn at render_size and halved by a linear blit, the same smoothing the still thumbnails get from their scale-down.
				QOpenGLFramebufferObjectFormat format;
				format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
				atlas = std::make_unique<QOpenGLFramebufferObject>(static_cast<int>(max_animated_tiles) * big, big, format);
				atlas_small = std::make_unique<QOpenGLFramebufferObject>(static_cast<int>(max_animated_tiles) * small, small);
				if (!atlas->isValid() || !atlas_small->isValid()) {
					throw std::runtime_error("animation framebuffer is not valid");
				}
			}

			if (!tile_vao) {
				glGenVertexArrays(1, &tile_vao);
			}
			glBindVertexArray(tile_vao);
			atlas->bind();
			begin_thumbnail_pass(count * big, big);

			for (int i = 0; i < count; ++i) {
				Tile& tile = *mine[i];
				try {
					const FrameAdvance step = next_tile_frame(tile.frame, tile.start, tile.end, delta_seconds);
					tile.frame = step.frame;
					tile.skeleton->current_frame = static_cast<int>(tile.frame);
					tile.skeleton->update(std::min(delta_seconds, max_tile_step_seconds), false, step.wrapped);
					glViewport(i * big, 0, big, big);
					draw(*tile.mesh, *tile.skeleton, tile.pose);
				} catch (...) {
					failed[i] = true;
				}
			}

			glFinish();
			QOpenGLFramebufferObject::blitFramebuffer(
				atlas_small.get(), QRect(0, 0, count * small, small), atlas.get(), QRect(0, 0, count * big, big), GL_COLOR_BUFFER_BIT, GL_LINEAR
			);
			atlas_image = atlas_small->toImage(); // flipped to top-down; the tiles sit side by side, so only rows move
			glBindVertexArray(0);
			atlas->release();
		} catch (...) {
			atlas_image = QImage();
			std::fill(failed.begin(), failed.end(), true); // no atlas: every tile goes back to its still thumbnail until the next stop_animation()
			atlas.reset();
			atlas_small.reset();
		}

		std::unordered_set<const Tile*> broken;
		for (int i = 0; i < count; ++i) {
			TileFrame frame { mine[i]->path, mine[i]->norm, {} };
			if (!failed[i] && !atlas_image.isNull()) {
				frame.image = atlas_image.copy(i * small, 0, small, small).convertToFormat(QImage::Format_RGB32);
			} else {
				broken.insert(mine[i]);
			}
			frames.push_back(std::move(frame));
		}
		// A tile that threw is skipped until the next stop_animation().
		if (!broken.empty()) {
			std::erase_if(tiles, [&](const Tile& tile) { return broken.contains(&tile); });
			if (tiles.empty()) {
				clear_tiles(evicted);
			} else {
				evicted += resource_manager.evict_unused_in(tile_keys);
			}
		}
		release();
		return frames;
	}

  private:
	struct Tile {
		const void* owner = nullptr; // the ModelThumbnailCache that plays it
		QString path;
		std::string norm;
		std::shared_ptr<mdx::MDX> mdx;
		std::shared_ptr<EditableMesh> mesh;
		std::unique_ptr<SkeletalModelInstance> skeleton;
		Pose pose {};
		double start = 0.0;
		double end = 0.0;
		double frame = 0.0;
	};

	/// Leaves the caller's context as we found it: a GL widget that paints next must not see ours.
	bool make_current(std::string& error) {
		previous_context = QOpenGLContext::currentContext();
		previous_surface = previous_context ? previous_context->surface() : nullptr;
		if (!context.makeCurrent(&surface)) {
			error = "could not make the thumbnail context current";
			restore_previous(); // a failed makeCurrent may have released the caller's context
			return false;
		}
		return true;
	}

	void release() {
		context.doneCurrent();
		restore_previous();
	}

	void restore_previous() {
		if (previous_context && previous_surface) {
			previous_context->makeCurrent(previous_surface);
		}
		previous_context = nullptr;
		previous_surface = nullptr;
	}

	void ensure_shaders() {
		if (!shader_sd) {
			shader_sd = resource_manager.load<Shader>({ "data/shaders/editable_mesh_sd.vert", "data/shaders/editable_mesh_sd.frag" }).value();
			shader_hd = resource_manager.load<Shader>({ "data/shaders/editable_mesh_hd.vert", "data/shaders/editable_mesh_hd.frag" }).value();
		}
	}

	/// SD and HD passes, opaque, transparent, particles: the HD pass draws only HD geosets and the SD pass only SD ones.
	void draw(const EditableMesh& mesh, const SkeletalModelInstance& skeleton, const Pose& pose) {
		const glm::mat4& pv = pose.projection_view;
		const glm::vec3& dir = pose.direction;
		shader_sd->use();
		mesh.render_opaque(false, 1, skeleton, pv, dir);
		shader_hd->use();
		mesh.render_opaque(true, 1, skeleton, pv, dir);

		glDepthMask(false);
		shader_sd->use();
		mesh.render_transparent(false, 1, skeleton, pv, dir);
		shader_hd->use();
		mesh.render_transparent(true, 1, skeleton, pv, dir);
		glDepthMask(true);

		const glm::vec3 up = { 0, 0, 1 };
		const glm::vec3 camera_right = glm::normalize(glm::cross(dir, up));
		const glm::vec3 camera_up = glm::normalize(glm::cross(camera_right, dir));
		mesh.render_particles(skeleton, pv, camera_right, camera_up, dir);
	}

	/// With our context current: every tile and the cache entries the tiles added that nothing else uses. The atlases (about 8 MB)
	/// stay for the next dialog: recreating GPU objects per dialog grew the driver's private memory.
	void clear_tiles(std::size_t& evicted) {
		tiles.clear();
		evicted += resource_manager.evict_unused_in(tile_keys);
		tile_keys.clear(); // whatever is still held belongs to someone else now
	}

	QOffscreenSurface surface;
	QOpenGLContext context;
	QOpenGLContext* previous_context = nullptr;
	QSurface* previous_surface = nullptr;
	std::shared_ptr<Shader> shader_sd;
	std::shared_ptr<Shader> shader_hd;

	std::vector<Tile> tiles;
	std::unordered_set<std::string> tile_keys; // cache entries the tiles added (see add_tile)
	std::unique_ptr<QOpenGLFramebufferObject> atlas;
	std::unique_ptr<QOpenGLFramebufferObject> atlas_small;
	GLuint tile_vao = 0;
};

ModelThumbnailCache::ModelThumbnailCache(const bool use_disk_cache, QObject* parent) : QObject(parent), use_disk_cache(use_disk_cache) {
	timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, &QTimer::timeout, this, &ModelThumbnailCache::tick);

	animation_timer = new QTimer(this);
	animation_timer->setTimerType(Qt::PreciseTimer);
	animation_timer->setInterval(1000 / animated_tile_fps);
	connect(animation_timer, &QTimer::timeout, this, &ModelThumbnailCache::animation_tick);

	QPixmapCache::setCacheLimit(std::max(QPixmapCache::cacheLimit(), 48 * 1024)); // KB: about 700 thumbnails

	// One stamp for every stock model: the game build. The Warcraft III folder's `.build.info` changes with each patch.
	const QFileInfo build_info(QString::fromStdWString((hierarchy.warcraft_directory / ".build.info").wstring()));
	game_stamp = build_info.exists() ? std::to_string(build_info.lastModified().toSecsSinceEpoch()) : "nobuild";

	if (use_disk_cache) {
		// One folder per rendering version and game build; the folders of older versions and builds are removed here.
		const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/model_thumbnails";
		const QString current = QString("v%1-%2").arg(model_thumbnail_version).arg(QString::fromStdString(game_stamp));
		disk_directory = root + "/" + current;
		const QDir root_dir(root);
		for (const QString& name : root_dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			if (name != current && name.size() > 1 && name[0] == 'v' && name[1].isDigit()) {
				QDir(root + "/" + name).removeRecursively();
			}
		}
	}
}

ModelThumbnailCache::~ModelThumbnailCache() {
	// std::async futures wait for their worker, which is at most one MDX parse.
	queue.clear();
	ready.clear();
	in_flight.clear();
	animation_jobs.clear();
	// The shared context stays; only this cache's tiles go (normally already gone through stop_animation).
	if (Renderer* gl = live_renderer()) {
		std::size_t evicted = 0;
		(void)gl->drop_tiles_except(this, {}, evicted);
	}
}

ModelThumbnailCache::Renderer* ModelThumbnailCache::shared_renderer = nullptr;

ModelThumbnailCache::Renderer* ModelThumbnailCache::live_renderer() {
	return shared_renderer;
}

ModelThumbnailCache::Renderer& ModelThumbnailCache::ensure_renderer() {
	if (!shared_renderer) {
		shared_renderer = new Renderer();
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
			delete shared_renderer;
			shared_renderer = nullptr;
		});
	}
	return *shared_renderer;
}

int ModelThumbnailCache::pending() const {
	return static_cast<int>(queue.size() + in_flight.size() + ready.size());
}

QPixmap ModelThumbnailCache::pixmap(const QString& path) const {
	const auto key = file_keys.find(model_catalog_key(path.toStdString()));
	if (key == file_keys.end()) {
		return {};
	}
	QPixmap result;
	if (QPixmapCache::find(memory_key(key->second), &result)) {
		return result;
	}
	return {};
}

bool ModelThumbnailCache::failed(const QString& path) const {
	return failed_paths.contains(model_catalog_key(path.toStdString()));
}

QPixmap ModelThumbnailCache::placeholder(const bool failed) {
	static QPixmap tiles[2];
	QPixmap& tile = tiles[failed ? 1 : 0];
	if (tile.isNull()) {
		tile = QPixmap(thumbnail_size, thumbnail_size);
		tile.fill(QColor(0x2a, 0x2d, 0x32));
		if (failed) {
			QPainter painter(&tile);
			painter.setRenderHint(QPainter::TextAntialiasing);
			QFont font = painter.font();
			font.setPixelSize(48);
			font.setBold(true);
			painter.setFont(font);
			painter.setPen(QColor(0x6b, 0x70, 0x78));
			painter.drawText(tile.rect(), Qt::AlignCenter, "!");
		}
	}
	return tile;
}

void ModelThumbnailCache::request(const QStringList& paths) {
	queue.clear();
	for (const QString& path : paths) {
		const std::string norm = model_catalog_key(path.toStdString());
		if (failed_paths.contains(norm)) {
			continue;
		}
		file_key(path, norm);
		if (done.contains(norm) && !pixmap(path).isNull()) {
			continue;
		}
		const bool already_working = std::ranges::any_of(in_flight, [&](const auto& job) { return job->norm == norm; })
			|| std::ranges::any_of(ready, [&](const Ready& item) { return item.norm == norm; });
		if (!already_working) {
			queue.push_back({ path, norm });
		}
	}
	if (pending() > 0) {
		schedule(false);
	}
}

void ModelThumbnailCache::cancel() {
	++generation;
	queue.clear();
	ready.clear();
	timer->stop();
}

std::string ModelThumbnailCache::file_key(const QString& path, const std::string& norm) {
	if (const auto it = file_keys.find(norm); it != file_keys.end()) {
		return it->second;
	}
	// A map model is stamped with its file's time, a stock one with the game build.
	std::string stamp = game_stamp;
	if (!hierarchy.map_directory.empty()) {
		std::error_code ec;
		const fs::path on_disk = hierarchy.map_directory / fs::path(path.toStdWString());
		if (fs::is_regular_file(on_disk, ec)) {
			stamp = std::to_string(fs::last_write_time(on_disk, ec).time_since_epoch().count());
		}
	}
	const std::string key = model_thumbnail_key(
		norm,
		ThumbnailContext { .hd = hierarchy.hd, .teen = hierarchy.teen, .tileset = hierarchy.tileset, .locale = hierarchy.locale },
		stamp
	);
	file_keys.emplace(norm, key);
	return key;
}

void ModelThumbnailCache::schedule(const bool waiting_on_workers) {
	if (!timer->isActive()) {
		timer->start(waiting_on_workers ? 8 : 0);
	}
}

void ModelThumbnailCache::store(const QString& path, const std::string& norm, const QImage& image, const bool write_to_disk) {
	const std::string key = file_key(path, norm);
	QPixmapCache::insert(memory_key(key), QPixmap::fromImage(image));
	done.insert(norm);
	if (write_to_disk && use_disk_cache) {
		QDir().mkpath(disk_directory);
		image.save(disk_directory + "/" + QString::fromStdString(key) + ".png", "PNG");
	}
	emit thumbnail_ready(path);
}

void ModelThumbnailCache::mark_failed(const QString& path, const std::string& norm, const std::string& why) {
	failed_paths.insert(norm);
	if (failure_messages.size() < 12) {
		failure_messages.push_back(path.toStdString() + ": " + why);
	}
	emit thumbnail_ready(path);
}

bool ModelThumbnailCache::try_disk(const QString& path, const std::string& norm) {
	if (!use_disk_cache) {
		return false;
	}
	QImage image;
	if (!image.load(disk_directory + "/" + QString::fromStdString(file_key(path, norm)) + ".png") || image.isNull()) {
		return false;
	}
	store(path, norm, image, false);
	++disk_count;
	return true;
}

void ModelThumbnailCache::harvest_jobs() {
	for (auto it = in_flight.begin(); it != in_flight.end();) {
		Job& job = **it;
		if (job.result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
			++it;
			continue;
		}
		auto [mdx, error] = job.result.get();
		if (job.generation == generation) {
			if (mdx) {
				ready.push_back(Ready { job.path, job.norm, std::move(mdx) });
			} else {
				mark_failed(job.path, job.norm, error);
			}
		}
		it = in_flight.erase(it);
	}
}

void ModelThumbnailCache::tick() {
	harvest_jobs();

	// Thumbnails that are already on disk cost one small PNG read each.
	int disk_loads = 0;
	while (!queue.empty() && disk_loads < max_disk_loads_per_tick && static_cast<int>(in_flight.size()) < max_parses_in_flight) {
		const Queued item = queue.front();
		if (failed_paths.contains(item.norm) || (done.contains(item.norm) && !pixmap(item.path).isNull())) {
			queue.pop_front();
			continue;
		}
		if (try_disk(item.path, item.norm)) {
			queue.pop_front();
			++disk_loads;
			continue;
		}
		break;
	}

	// Start parses on workers. The worker reads and parses only; it never touches GL or Qt.
	while (!queue.empty() && static_cast<int>(in_flight.size()) < max_parses_in_flight) {
		Queued item = std::move(queue.front());
		queue.pop_front();
		if (failed_paths.contains(item.norm) || (done.contains(item.norm) && !pixmap(item.path).isNull())) {
			continue;
		}
		if (try_disk(item.path, item.norm)) {
			continue;
		}

		auto job = std::make_unique<Job>();
		job->path = item.path;
		job->norm = item.norm;
		job->generation = generation;
		job->result = parse_async(item.path);
		in_flight.push_back(std::move(job));
	}

	// Draw a few on the main thread.
	for (int drawn = 0; drawn < max_renders_per_tick && !ready.empty(); ++drawn) {
		Ready item = std::move(ready.front());
		ready.erase(ready.begin());

		std::string error;
		std::size_t evicted = 0;
		const QImage image = ensure_renderer().render(item.mdx, error, evicted);
		evicted_count += static_cast<int>(evicted);
		item.mdx.reset();
		if (image.isNull()) {
			mark_failed(item.path, item.norm, error);
		} else {
			store(item.path, item.norm, image, true);
			++generated_count;
		}
	}

	if (pending() > 0) {
		schedule(ready.empty() && queue.empty());
	} else {
		emit idle();
	}
}

// --- Animated tiles -----------------------------------------------------------------------------------------------------------------------

void ModelThumbnailCache::animate(const QStringList& paths) {
	if (!animate_browse_tiles) {
		return;
	}
	animation_wanted.clear();
	std::unordered_set<std::string> keep;
	for (const QString& path : paths) {
		std::string norm = model_catalog_key(path.toStdString());
		if (failed_paths.contains(norm) || still_only.contains(norm) || temporarily_failed.contains(norm) || keep.contains(norm)) {
			continue;
		}
		keep.insert(norm);
		animation_wanted.push_back({ path, std::move(norm) });
	}

	if (Renderer* gl = live_renderer()) {
		std::size_t evicted = 0;
		for (const QString& path : gl->drop_tiles_except(this, keep, evicted)) {
			animation_frames_by_norm.erase(model_catalog_key(path.toStdString()));
			emit animation_frame(path);
		}
		evicted_count += static_cast<int>(evicted);
	}

	if (animation_wanted.empty()) {
		animation_timer->stop();
		return;
	}
	if (!animation_timer->isActive()) {
		animation_clock.start();
		animation_timer->start();
	}
}

void ModelThumbnailCache::pause_animation() {
	animation_timer->stop();
}

void ModelThumbnailCache::stop_animation() {
	animation_timer->stop();
	animation_wanted.clear();
	temporarily_failed.clear();
	if (Renderer* gl = live_renderer()) {
		std::size_t evicted = 0;
		for (const QString& path : gl->drop_tiles_except(this, {}, evicted)) {
			animation_frames_by_norm.erase(model_catalog_key(path.toStdString()));
			emit animation_frame(path);
		}
		evicted_count += static_cast<int>(evicted);
	}
	animation_frames_by_norm.clear();
}

QPixmap ModelThumbnailCache::animated_pixmap(const QString& path) const {
	if (animation_frames_by_norm.empty()) {
		return {};
	}
	const auto it = animation_frames_by_norm.find(model_catalog_key(path.toStdString()));
	return it == animation_frames_by_norm.end() ? QPixmap() : it->second;
}

int ModelThumbnailCache::animated_tiles() const {
	const Renderer* gl = live_renderer();
	return gl ? gl->tile_count(this) : 0;
}

void ModelThumbnailCache::animation_tick() {
	const double delta = static_cast<double>(animation_clock.restart()) / 1000.0;
	const auto wanted = [&](const std::string& norm) {
		return std::ranges::any_of(animation_wanted, [&](const Queued& item) { return item.norm == norm; });
	};
	Renderer& gl = ensure_renderer();
	std::size_t evicted = 0;

	// Parsed models become playing tiles, a couple per tick (each one creates a mesh and loads its textures on this thread).
	int added = 0;
	for (auto it = animation_jobs.begin(); it != animation_jobs.end() && added < max_renders_per_tick;) {
		Job& job = **it;
		if (job.result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
			++it;
			continue;
		}
		auto [mdx, error] = job.result.get();
		if (mdx && wanted(job.norm) && !gl.has_tile(this, job.norm)) {
			const Renderer::TileResult result = gl.add_tile(this, job.path, job.norm, mdx, evicted);
			if (result == Renderer::TileResult::StillOnly) {
				still_only.insert(job.norm);
			} else if (result == Renderer::TileResult::Failed) {
				temporarily_failed.insert(job.norm); // retried after the next stop_animation
			}
			++added;
		} else if (!mdx) {
			still_only.insert(job.norm);
		}
		it = animation_jobs.erase(it);
	}

	// New parses only once the still thumbnails are done, so the tiles first get their picture.
	bool waiting = !animation_jobs.empty();
	for (const Queued& item : animation_wanted) {
		if (gl.has_tile(this, item.norm) || still_only.contains(item.norm) || temporarily_failed.contains(item.norm) || failed_paths.contains(item.norm)) {
			continue;
		}
		waiting = true;
		if (pending() > 0 || static_cast<int>(animation_jobs.size()) >= max_parses_in_flight
			|| std::ranges::any_of(animation_jobs, [&](const auto& job) { return job->norm == item.norm; })) {
			continue;
		}
		auto job = std::make_unique<Job>();
		job->path = item.path;
		job->norm = item.norm;
		job->generation = generation;
		job->result = parse_async(item.path);
		animation_jobs.push_back(std::move(job));
	}

	for (Renderer::TileFrame& frame : gl.render_tiles(this, delta, evicted)) {
		if (frame.image.isNull()) {
			temporarily_failed.insert(frame.norm);
			animation_frames_by_norm.erase(frame.norm);
		} else {
			animation_frames_by_norm[frame.norm] = QPixmap::fromImage(frame.image);
			++animation_frame_count;
		}
		emit animation_frame(frame.path);
	}
	evicted_count += static_cast<int>(evicted);

	if (gl.tile_count(this) == 0 && !waiting) {
		animation_timer->stop(); // nothing in view can play
	}
}
