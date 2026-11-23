#include <glad/glad.h>

#include "model_thumbnail_cache.h"

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
import <glm/gtc/matrix_transform.hpp>;

namespace fs = std::filesystem;

namespace {
	constexpr float k_fov_deg = 50.f;
	constexpr float k_near = 0.05f;
	constexpr float k_far = 20000.f;

	QString memory_key(const std::string& file_key) {
		return QString("hwe-thumb:") + QString::fromStdString(file_key);
	}
} // namespace

struct ModelThumbnailCache::Job {
	QString path;
	std::string norm;
	int generation = 0;
	std::future<std::pair<std::shared_ptr<mdx::MDX>, std::string>> result;
};

struct ModelThumbnailCache::Ready {
	QString path;
	std::string norm;
	std::shared_ptr<mdx::MDX> mdx;
};

/// The private offscreen context and everything created in it. Created on first use, on the main thread.
class ModelThumbnailCache::Renderer {
  public:
	Renderer() {
		surface.setFormat(QSurfaceFormat::defaultFormat());
		surface.create();
		context.setFormat(QSurfaceFormat::defaultFormat());
		context.setShareContext(QOpenGLContext::globalShareContext());
		usable = surface.isValid() && context.create();
	}

	bool usable = false;

	/// Null image with `error` filled in when the model cannot be drawn.
	/// `evicted` is increased by the number of cached resources (the model's textures) released again after the draw.
	QImage render(const std::shared_ptr<mdx::MDX>& mdx, std::string& error, std::size_t& evicted) {
		if (!usable) {
			error = "no OpenGL context for thumbnails";
			return {};
		}

		// Leave the caller's context as we found it: a GL widget that paints next must not see ours.
		QOpenGLContext* previous_context = QOpenGLContext::currentContext();
		QSurface* previous_surface = previous_context ? previous_context->surface() : nullptr;

		if (!context.makeCurrent(&surface)) {
			error = "could not make the thumbnail context current";
			if (previous_context && previous_surface) {
				previous_context->makeCurrent(previous_surface); // a failed makeCurrent may have released the caller's context
			}
			return {};
		}

		QImage image;
		GLuint vao = 0;
		std::unordered_set<std::string> cached_before;
		bool snapshot_taken = false;
		try {
			if (!shader_sd) {
				shader_sd = resource_manager.load<Shader>({ "data/shaders/editable_mesh_sd.vert", "data/shaders/editable_mesh_sd.frag" }).value();
				shader_hd = resource_manager.load<Shader>({ "data/shaders/editable_mesh_hd.vert", "data/shaders/editable_mesh_hd.frag" }).value();
			}
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
			glViewport(0, 0, ModelThumbnailCache::render_size, ModelThumbnailCache::render_size);
			glDisable(GL_SCISSOR_TEST);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
			glEnable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthMask(true);
			glClearColor(0.20f, 0.22f, 0.25f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

				float fit_distance = 200.f;
				glm::vec3 fit_position(0.f);
				if (!mdx->sequences.empty() && skeleton.sequence_index >= 0 && skeleton.sequence_index < static_cast<int>(mdx->sequences.size())) {
					auto extent = mdx->sequences[skeleton.sequence_index].extent;
					if (!(glm::length(extent.maximum - extent.minimum) > 0.001f)) {
						extent = mdx->extent;
					}
					const glm::vec3 size = extent.maximum - extent.minimum;
					const float radius = glm::length(size) * 0.5f;
					if (std::isfinite(radius) && radius > 0.001f) {
						fit_distance = radius / std::sin(glm::radians(k_fov_deg) * 0.5f);
						fit_position = glm::vec3(0.f, 0.f, extent.minimum.z + size.z * 0.5f);
					}
				}

				skeleton.update_location(glm::vec3(0.f), glm::quat(), glm::vec3(1.f));
				// A node can come before its parent in the list, so one update leaves its matrix a step behind: update until it settles.
				for (int pass = 0; pass < 3; ++pass) {
					skeleton.update(0.0, false, false);
				}

				const glm::vec3 dir = glm::normalize(glm::vec3 { -1.f, 1.f, -0.5f });
				const glm::vec3 up = { 0, 0, 1 };
				const glm::vec3 eye = fit_position - dir * fit_distance;
				const glm::mat4 pv = glm::perspective(glm::radians(k_fov_deg), 1.f, k_near, k_far) * glm::lookAt(eye, fit_position, up);

				shader_sd->use();
				mesh->render_opaque(false, 1, skeleton, pv, dir);
				shader_hd->use();
				mesh->render_opaque(true, 1, skeleton, pv, dir);

				glDepthMask(false);
				shader_sd->use();
				mesh->render_transparent(false, 1, skeleton, pv, dir);
				shader_hd->use();
				mesh->render_transparent(true, 1, skeleton, pv, dir);
				glDepthMask(true);

				const glm::vec3 camera_right = glm::normalize(glm::cross(dir, up));
				const glm::vec3 camera_up = glm::normalize(glm::cross(camera_right, dir));
				mesh->render_particles(skeleton, pv, camera_right, camera_up, dir);

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
		context.doneCurrent();
		if (previous_context && previous_surface) {
			previous_context->makeCurrent(previous_surface);
		}

		if (image.isNull()) {
			if (error.empty()) {
				error = "nothing was drawn";
			}
			return {};
		}
		return image.convertToFormat(QImage::Format_RGB32)
			.scaled(ModelThumbnailCache::thumbnail_size, ModelThumbnailCache::thumbnail_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}

  private:
	QOffscreenSurface surface;
	QOpenGLContext context;
	std::shared_ptr<Shader> shader_sd;
	std::shared_ptr<Shader> shader_hd;
};

ModelThumbnailCache::ModelThumbnailCache(const bool use_disk_cache, QObject* parent) : QObject(parent), use_disk_cache(use_disk_cache) {
	timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, &QTimer::timeout, this, &ModelThumbnailCache::tick);

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
	renderer.reset();
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
		const std::wstring name = item.path.toStdWString();
		job->result = std::async(std::launch::async, [name]() -> std::pair<std::shared_ptr<mdx::MDX>, std::string> {
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
		in_flight.push_back(std::move(job));
	}

	// Draw a few on the main thread.
	for (int drawn = 0; drawn < max_renders_per_tick && !ready.empty(); ++drawn) {
		Ready item = std::move(ready.front());
		ready.erase(ready.begin());

		if (!renderer) {
			renderer = std::make_unique<Renderer>();
		}
		std::string error;
		std::size_t evicted = 0;
		const QImage image = renderer->render(item.mdx, error, evicted);
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
