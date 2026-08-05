#include "map_protector.h"

import std;
import Map;
import MapGlobal;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QMetaObject>
#include <QDir>

namespace {
	QCheckBox* make_check(QGroupBox* group, QVBoxLayout* layout, const QString& text, const QString& tooltip, bool checked) {
		auto* check = new QCheckBox(text, group);
		check->setChecked(checked);
		check->setToolTip(tooltip);
		layout->addWidget(check);
		return check;
	}
} // namespace

MapProtector::MapProtector(QWidget* parent) : QMainWindow(parent) {
	setWindowTitle("Map Protector");
	resize(700, 560);

	auto* central = new QWidget(this);
	setCentralWidget(central);
	auto* main_layout = new QVBoxLayout(central);

	// Output path
	auto* output_row = new QHBoxLayout;
	output_row->addWidget(new QLabel("Output file:", central));
	output_path_edit = new QLineEdit(central);
	browse_button = new QPushButton("Browse...", central);
	output_row->addWidget(output_path_edit, 1);
	output_row->addWidget(browse_button);
	main_layout->addLayout(output_row);

	// MPQ archive hardening
	auto* mpq_group = new QGroupBox("MPQ Archive Hardening", central);
	auto* mpq_layout = new QVBoxLayout(mpq_group);
	remove_listfile_check = make_check(mpq_group, mpq_layout, "Remove listfile", "Deletes (listfile) from the MPQ so editors cannot enumerate files.", true);
	remove_attributes_check = make_check(mpq_group, mpq_layout, "Remove attributes file", "Deletes (attributes), the MD5 hash table used by some tools.", true);
	main_layout->addWidget(mpq_group);

	// Trigger hardening
	auto* trigger_group = new QGroupBox("Trigger / Script Hardening", central);
	auto* trigger_layout = new QVBoxLayout(trigger_group);
	remove_gui_triggers_check = make_check(trigger_group, trigger_layout, "Remove GUI trigger data", "Deletes war3map.wtg (trigger tree). The map runs on its compiled script; editors show nothing.", true);
	main_layout->addWidget(trigger_group);

	// Metadata sanitization
	auto* metadata_group = new QGroupBox("Metadata Sanitization", central);
	auto* metadata_layout = new QVBoxLayout(metadata_group);
	clear_author_check = make_check(metadata_group, metadata_layout, "Clear map author", "Blanks the author field in war3map.w3i.", false);
	clear_description_check = make_check(metadata_group, metadata_layout, "Clear map description", "Blanks the description field.", false);
	clear_loading_text_check = make_check(metadata_group, metadata_layout, "Clear loading screen text", "Blanks the loading screen title, subtitle, and body text.", false);
	normalize_name_check = make_check(metadata_group, metadata_layout, "Normalize map name", "Replaces the map name with a generic placeholder.", false);
	main_layout->addWidget(metadata_group);

	main_layout->addStretch(1);

	// Status / action bar
	auto* action_row = new QHBoxLayout;
	status_label = new QLabel("Ready", central);
	progress_bar = new QProgressBar(central);
	progress_bar->setRange(0, 0);
	progress_bar->setVisible(false);
	progress_bar->setMaximumWidth(160);
	export_button = new QPushButton("Export Protected Map", central);
	action_row->addWidget(status_label, 1);
	action_row->addWidget(progress_bar);
	action_row->addWidget(export_button);
	main_layout->addLayout(action_row);

	connect(browse_button, &QPushButton::clicked, this, &MapProtector::on_browse_clicked);
	connect(export_button, &QPushButton::clicked, this, &MapProtector::on_export_clicked);

	load_settings();
	populate_default_output_path();
}

void MapProtector::showEvent(QShowEvent* event) {
	QMainWindow::showEvent(event);
	populate_default_output_path();
}

void MapProtector::populate_default_output_path() {
	if (!output_path_edit->text().isEmpty() || !map || !map->loaded) {
		return;
	}

	// Default next to the currently loaded map, not the app-wide "openDirectory" setting: that
	// setting is shared by every open/save dialog in HiveWE and for folder-expansion maps holds
	// the last-opened map's own folder (see HiveWE::load_folder()), which may be a stale path
	// from an unrelated map and isn't guaranteed to exist.
	QString directory;
	if (!map->filesystem_path.empty()) {
		// Map::load()/save() always leave filesystem_path with a trailing separator, which makes
		// filename() empty and a single parent_path() a no-op (it just strips that separator and
		// returns the map's own folder again). Strip it explicitly so parent_path() below lands
		// one level up, next to the map's folder, instead of inside it.
		fs::path map_folder = map->filesystem_path;
		if (map_folder.filename().empty()) {
			map_folder = map_folder.parent_path();
		}
		directory = QString::fromStdWString(map_folder.parent_path().wstring());
	} else {
		QSettings settings;
		directory = settings.value("openDirectory", QDir::current().path()).toString();
	}
	output_path_edit->setText(directory + "/" + QString::fromStdString(map->name) + "_protected.w3x");
}

void MapProtector::closeEvent(QCloseEvent* event) {
	if (export_in_progress) {
		event->ignore();
		return;
	}
	save_settings();
	QMainWindow::closeEvent(event);
}

void MapProtector::on_browse_clicked() {
	const QString start = output_path_edit->text();
	const QString path = QFileDialog::getSaveFileName(this, "Export Protected Map", start, "Warcraft III Scenario (*.w3x)");
	if (!path.isEmpty()) {
		output_path_edit->setText(path);
	}
}

ProtectionOptions MapProtector::collect_options() const {
	ProtectionOptions options;
	options.remove_listfile = remove_listfile_check->isChecked();
	options.remove_attributes = remove_attributes_check->isChecked();
	options.remove_gui_triggers = remove_gui_triggers_check->isChecked();
	options.clear_author = clear_author_check->isChecked();
	options.clear_description = clear_description_check->isChecked();
	options.clear_loading_text = clear_loading_text_check->isChecked();
	options.normalize_name = normalize_name_check->isChecked();
	return options;
}

void MapProtector::set_options_enabled(bool enabled) {
	const std::initializer_list<QWidget*> widgets = {
		remove_listfile_check, remove_attributes_check, remove_gui_triggers_check,
		clear_author_check, clear_description_check, clear_loading_text_check, normalize_name_check,
		output_path_edit, browse_button, export_button
	};
	for (QWidget* widget : widgets) {
		widget->setEnabled(enabled);
	}
}

void MapProtector::on_export_clicked() {
	if (!map || !map->loaded) {
		QMessageBox::warning(this, "Map Protector", "No map is loaded.");
		return;
	}

	const QString output = output_path_edit->text();
	if (output.isEmpty()) {
		QMessageBox::warning(this, "Map Protector", "Choose an output file first.");
		return;
	}

	const ProtectionOptions options = collect_options();
	save_settings();

	set_options_enabled(false);
	status_label->setText("Saving map data...");
	progress_bar->setVisible(true);

	temp_dir = std::make_unique<QTemporaryDir>();
	if (!temp_dir->isValid()) {
		status_label->setText("Failed to create a temporary folder.");
		set_options_enabled(true);
		progress_bar->setVisible(false);
		temp_dir.reset();
		return;
	}

	const fs::path temp_path = temp_dir->path().toStdWString();
	const SyncSaveResult save_result = run_sync_save_and_restore(temp_path, options);
	if (!save_result.success) {
		status_label->setText(QString::fromStdString("Error: " + save_result.error));
		set_options_enabled(true);
		progress_bar->setVisible(false);
		temp_dir.reset();
		return;
	}

	status_label->setText("Packing protected map...");
	export_in_progress = true;

	const fs::path output_path = output.toStdWString();

	pack_thread = QThread::create([this, temp_path, output_path, options]() {
		const PackResult result = run_async_pack(temp_path, output_path, options);
		QMetaObject::invokeMethod(this, [this, result]() { on_export_finished(result); }, Qt::QueuedConnection);
	});
	pack_thread->start();
}

void MapProtector::on_export_finished(PackResult result) {
	export_in_progress = false;
	progress_bar->setVisible(false);
	set_options_enabled(true);

	if (result.success) {
		status_label->setText("Done - saved to " + output_path_edit->text());
	} else {
		status_label->setText(QString::fromStdString("Error: " + result.error));
	}

	temp_dir.reset();

	if (pack_thread) {
		pack_thread->wait();
		pack_thread->deleteLater();
		pack_thread = nullptr;
	}
}

void MapProtector::load_settings() {
	QSettings settings;
	settings.beginGroup("MapProtector");
	output_path_edit->setText(settings.value("outputPath", "").toString());
	remove_listfile_check->setChecked(settings.value("removeListfile", true).toBool());
	remove_attributes_check->setChecked(settings.value("removeAttributes", true).toBool());
	remove_gui_triggers_check->setChecked(settings.value("removeGuiTriggers", true).toBool());
	clear_author_check->setChecked(settings.value("clearAuthor", false).toBool());
	clear_description_check->setChecked(settings.value("clearDescription", false).toBool());
	clear_loading_text_check->setChecked(settings.value("clearLoadingText", false).toBool());
	normalize_name_check->setChecked(settings.value("normalizeName", false).toBool());
	settings.endGroup();
}

void MapProtector::save_settings() const {
	QSettings settings;
	settings.beginGroup("MapProtector");
	settings.setValue("outputPath", output_path_edit->text());
	settings.setValue("removeListfile", remove_listfile_check->isChecked());
	settings.setValue("removeAttributes", remove_attributes_check->isChecked());
	settings.setValue("removeGuiTriggers", remove_gui_triggers_check->isChecked());
	settings.setValue("clearAuthor", clear_author_check->isChecked());
	settings.setValue("clearDescription", clear_description_check->isChecked());
	settings.setValue("clearLoadingText", clear_loading_text_check->isChecked());
	settings.setValue("normalizeName", normalize_name_check->isChecked());
	settings.endGroup();
}
