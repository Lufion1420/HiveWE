#include "model_view.h"

#include "model_editor.h"
#include "model_grid_glwidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

import std;
import Hierarchy;
import Globals;
import WindowHandler;
import Utilities;
import ModelCatalog;

namespace fs = std::filesystem;

ModelView::ModelView(QWidget* parent) : QWidget(parent) {
	// Enumerate through Hierarchy; ModelCatalog decides what is a model, which category it is in and which duplicate wins.
	std::vector<std::string> map_files;
	if (!hierarchy.map_directory.empty() && fs::exists(hierarchy.map_directory)) {
		std::error_code ec;
		for (const auto& it : fs::recursive_directory_iterator(hierarchy.map_directory, ec)) {
			if (ec) {
				break;
			}
			if (it.is_regular_file()) {
				map_files.push_back(it.path().lexically_relative(hierarchy.map_directory).string());
			}
		}
	}

	std::vector<std::string> game_files;
	for (const auto& src : game_category_sources) {
		auto names = hierarchy.game_data.find_files(std::string(src.glob));
		game_files.insert(game_files.end(), std::make_move_iterator(names.begin()), std::make_move_iterator(names.end()));
	}
	const std::vector<ModelEntry> entries = build_model_catalog(map_files, game_files).entries;

	QVBoxLayout* layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(search);

	preview_animation->addItems({"Stand", "Spell", "Attack", "Birth", "Death"});
	preview_animation->setCurrentText("Stand");

	QHBoxLayout* preview_row = new QHBoxLayout;
	preview_row->setContentsMargins(0, 0, 0, 0);
	preview_row->addWidget(new QLabel("Preview Animation"));
	preview_row->addWidget(preview_animation, 1);
	layout->addLayout(preview_row);

	QHBoxLayout* category_row = new QHBoxLayout;
	category_row->setContentsMargins(0, 0, 0, 0);
	std::vector<QCheckBox*> category_boxes(static_cast<size_t>(ModelCategory::Count), nullptr);
	for (const auto& src : game_category_sources) {
		auto* cb = new QCheckBox(QString::fromUtf8(src.label.data(), static_cast<qsizetype>(src.label.size())));
		cb->setChecked(true);
		category_boxes[static_cast<size_t>(src.category)] = cb;
		category_row->addWidget(cb);
	}

	{
		auto* cb = new QCheckBox("Map");
		cb->setChecked(true);
		category_boxes[static_cast<size_t>(ModelCategory::Map)] = cb;
		category_row->addWidget(cb);
	}
	category_row->addStretch(1);
	layout->addLayout(category_row);

	ModelGridGLWidget* grid = new ModelGridGLWidget(entries, this);
	QScrollBar* bar = new QScrollBar(Qt::Vertical, this);

	QHBoxLayout* grid_row = new QHBoxLayout;
	grid_row->setContentsMargins(0, 0, 0, 0);
	grid_row->setSpacing(0);
	grid_row->addWidget(grid, 1);
	grid_row->addWidget(bar);
	layout->addLayout(grid_row, 1);

	connect(bar, &QScrollBar::valueChanged, grid, &ModelGridGLWidget::set_scroll_offset);
	connect(grid, &ModelGridGLWidget::scroll_changed, bar, &QScrollBar::setValue);
	connect(grid, &ModelGridGLWidget::content_height_changed, this, [bar, grid](int total) {
		bar->setRange(0, std::max(0, total - grid->height()));
		bar->setPageStep(grid->height());
		bar->setSingleStep(grid->cell_pixel_size());
	});
	connect(grid, &ModelGridGLWidget::clicked, this, [this](const fs::path& p) {
		finalPath->setText(QString::fromStdString(p.string()));
	});

	connect(grid, &ModelGridGLWidget::double_clicked, this, [this](const fs::path& p) {
		emit doubleClicked(p);
	});

	connect(search, &QLineEdit::textChanged, grid, &ModelGridGLWidget::set_search);
	connect(preview_animation, &QComboBox::currentTextChanged, grid, &ModelGridGLWidget::set_preview_sequence);

	auto update_categories = [grid, category_boxes]() {
		std::bitset<static_cast<size_t>(ModelCategory::Count)> mask;
		for (size_t i = 0; i < category_boxes.size(); ++i) {
			if (category_boxes[i] && category_boxes[i]->isChecked()) {
				mask.set(i);
			}
		}
		grid->set_categories(mask);
	};
	for (auto* cb : category_boxes) {
		if (cb) {
			connect(cb, &QCheckBox::toggled, this, [update_categories](bool) {
				update_categories();
			});
		}
	}

	open_in_model_editor->setIcon(QIcon("data/icons/ribbon/model_editor.png"));
	open_in_model_editor->setIconSize(QSize(20, 20));
	open_in_model_editor->setToolTip("Open in Model Editor");

	QHBoxLayout* hlayout = new QHBoxLayout;
	hlayout->addWidget(new QLabel("Path"));
	hlayout->addWidget(finalPath);
	hlayout->addWidget(open_in_model_editor);
	layout->addLayout(hlayout);
	setLayout(layout);

	search->setPlaceholderText("Search Models");
	grid->set_preview_sequence(preview_animation->currentText());

	connect(open_in_model_editor, &QPushButton::clicked, [this] {
		if (finalPath->text().isEmpty()) {
			return;
		}

		bool created = false;
		const auto model_editor = window_handler.create_or_raise<ModelEditor>(nullptr, created);

		auto path = fs::path(finalPath->text().toStdString());
		path.replace_extension(".mdx");

		// show_model reports a failed open itself and adds the tab to the editor.
		model_editor->show_model(path, false);
	});
}

QString ModelView::current_model_path() {
	return finalPath->text();
}

void ModelView::set_current_model_path(const QString& path) {
	finalPath->setText(path);
}
