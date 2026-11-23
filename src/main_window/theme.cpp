#include "theme.h"

#include <QApplication>
#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QSettings>
#include <QStandardPaths>
#include <QSvgRenderer>

import std;
import ThemeTokens;

namespace {
QString active_theme;
bool active_is_flat = false;
QColor icon_color(0xdf, 0xe1, 0xe5);
QColor icon_disabled_color(0x5c, 0x5f, 0x66);

// The application palette and font before the first Flat theme, restored when an old theme is selected again.
bool defaults_saved = false;
QPalette default_palette;
QFont default_font;
bool font_loaded = false;
QString font_family;

std::optional<QByteArray> read_file(const QString& path) {
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	return file.readAll();
}

QColor token_color(const theme_tokens::TokenMap& tokens, const std::string_view name) {
	const auto token = tokens.find(name);
	return token == tokens.end() ? QColor() : QColor(QString::fromStdString(token->second));
}

/// "Inter" from data/fonts, loaded once. The family name stays empty when the files are missing, then the system font is used.
void load_font() {
	if (font_loaded) {
		return;
	}
	font_loaded = true;

	for (const char* file : { "data/fonts/Inter-Regular.ttf", "data/fonts/Inter-SemiBold.ttf" }) {
		const int id = QFontDatabase::addApplicationFont(file);
		if (id < 0) {
			qWarning() << "Loading the font" << file << "failed";
			continue;
		}
		if (font_family.isEmpty()) {
			const QStringList families = QFontDatabase::applicationFontFamilies(id);
			if (!families.isEmpty()) {
				font_family = families.first();
			}
		}
	}
}

QPalette flat_palette(const theme_tokens::TokenMap& tokens) {
	const auto color = [&](const std::string_view name) { return token_color(tokens, name); };

	QPalette palette;
	palette.setColor(QPalette::Window, color("color-window"));
	palette.setColor(QPalette::WindowText, color("color-text"));
	palette.setColor(QPalette::Base, color("color-base"));
	palette.setColor(QPalette::AlternateBase, color("color-alt-base"));
	palette.setColor(QPalette::ToolTipBase, color("color-menu-bg"));
	palette.setColor(QPalette::ToolTipText, color("color-text"));
	palette.setColor(QPalette::Text, color("color-text"));
	palette.setColor(QPalette::PlaceholderText, color("color-text-muted"));
	palette.setColor(QPalette::Button, color("color-button-top"));
	palette.setColor(QPalette::ButtonText, color("color-text"));
	palette.setColor(QPalette::BrightText, color("color-error"));
	palette.setColor(QPalette::Link, color("color-accent"));
	palette.setColor(QPalette::Highlight, color("color-selection"));
	palette.setColor(QPalette::HighlightedText, color("color-selection-text"));
	palette.setColor(QPalette::Light, color("color-hover"));
	palette.setColor(QPalette::Midlight, color("color-window"));
	palette.setColor(QPalette::Mid, color("color-border"));
	palette.setColor(QPalette::Dark, color("color-border-strong"));
	palette.setColor(QPalette::Shadow, color("color-border-strong"));

	for (const auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText }) {
		palette.setColor(QPalette::Disabled, role, color("color-text-disabled"));
	}
	palette.setColor(QPalette::Disabled, QPalette::HighlightedText, color("color-text-disabled"));
	return palette;
}

/// Writes the small PNGs the style sheet refers to as url(@{asset-dir}/...): Qt style sheets cannot recolour an image, so they are drawn
/// per theme. Returns false when the folder or a file cannot be written.
bool write_assets(const QString& directory, const theme_tokens::TokenMap& tokens) {
	if (!QDir().mkpath(directory)) {
		return false;
	}

	const QColor arrow = token_color(tokens, "color-text");
	const QColor check = token_color(tokens, "color-accent-text");

	// A triangle pointing in `direction`: 0 down, 1 up, 2 right.
	const auto arrow_image = [&](const int direction) {
		QImage image(32, 32, QImage::Format_ARGB32_Premultiplied);
		image.fill(Qt::transparent);
		QPainter painter(&image);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);
		painter.setBrush(arrow);
		QPolygonF triangle;
		switch (direction) {
			case 0:
				triangle << QPointF(6, 11) << QPointF(26, 11) << QPointF(16, 22);
				break;
			case 1:
				triangle << QPointF(6, 21) << QPointF(26, 21) << QPointF(16, 10);
				break;
			default:
				triangle << QPointF(11, 6) << QPointF(11, 26) << QPointF(22, 16);
				break;
		}
		painter.drawPolygon(triangle);
		return image;
	};

	QImage check_image(28, 28, QImage::Format_ARGB32_Premultiplied);
	check_image.fill(Qt::transparent);
	{
		QPainter painter(&check_image);
		painter.setRenderHint(QPainter::Antialiasing);
		QPen pen(check, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
		painter.setPen(pen);
		painter.drawPolyline(QPolygonF { QPointF(7, 14.5), QPointF(12, 19.5), QPointF(21, 8.5) });
	}

	// The dot of a checked radio button; the ring is the border of the indicator.
	QImage radio_image(28, 28, QImage::Format_ARGB32_Premultiplied);
	radio_image.fill(Qt::transparent);
	{
		QPainter painter(&radio_image);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);
		painter.setBrush(token_color(tokens, "color-accent"));
		painter.drawEllipse(QPointF(14, 14), 6.0, 6.0);
	}

	return radio_image.save(directory + "/radio-dot.png") && arrow_image(0).save(directory + "/arrow-down.png") && arrow_image(1).save(directory + "/arrow-up.png")
		&& arrow_image(2).save(directory + "/arrow-right.png") && check_image.save(directory + "/check.png");
}

/// The style sheet of a Flat theme, or the error text.
std::expected<QString, QString> build_flat_stylesheet(const QString& name, theme_tokens::TokenMap& tokens_out) {
	const auto token_text = read_file("data/themes/Flat.tokens");
	if (!token_text) {
		return std::unexpected(QStringLiteral("Reading data/themes/Flat.tokens failed"));
	}
	const auto template_text = read_file("data/themes/Flat.qss.in");
	if (!template_text) {
		return std::unexpected(QStringLiteral("Reading data/themes/Flat.qss.in failed"));
	}

	const std::string_view section = name == theme::flat_light ? "light" : "dark";
	auto tokens = theme_tokens::parse_tokens(std::string_view(token_text->constData(), token_text->size()), section);
	if (!tokens) {
		return std::unexpected("Flat.tokens: " + QString::fromStdString(tokens.error()));
	}

	const QString assets = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/theme-assets/" + QString::fromUtf8(section.data(), static_cast<qsizetype>(section.size()));
	if (!write_assets(assets, *tokens)) {
		return std::unexpected("Writing the theme images to " + assets + " failed");
	}
	(*tokens)["asset-dir"] = QDir::fromNativeSeparators(assets).toStdString();

	const auto stylesheet = theme_tokens::substitute_tokens(std::string_view(template_text->constData(), template_text->size()), *tokens);
	if (!stylesheet) {
		return std::unexpected("Flat.qss.in: " + QString::fromStdString(stylesheet.error()));
	}

	tokens_out = std::move(*tokens);
	return QString::fromStdString(*stylesheet);
}
}

namespace theme {
bool is_flat(const QString& name) {
	return name == flat_dark || name == flat_light;
}

const QString& current() {
	return active_theme;
}

bool current_is_flat() {
	return active_is_flat;
}

QString apply(const QString& name) {
	if (!defaults_saved) {
		defaults_saved = true;
		default_palette = QApplication::palette();
		default_font = QApplication::font();
	}

	if (!is_flat(name)) {
		const auto stylesheet = read_file("data/themes/" + name + ".qss");
		if (!stylesheet) {
			return "Reading the theme " + name + " failed";
		}

		QApplication::setPalette(default_palette);
		QApplication::setFont(default_font);
		qApp->setStyleSheet(QLatin1String(*stylesheet));
		active_theme = name;
		active_is_flat = false;
		return {};
	}

	theme_tokens::TokenMap tokens;
	const auto stylesheet = build_flat_stylesheet(name, tokens);
	if (!stylesheet) {
		return stylesheet.error();
	}

	load_font();
	QFont font = default_font;
	if (!font_family.isEmpty()) {
		font.setFamily(font_family);
	}
	font.setPixelSize(12);
	QApplication::setFont(font);

	QApplication::setPalette(flat_palette(tokens));
	qApp->setStyleSheet(*stylesheet);

	icon_color = token_color(tokens, "color-text");
	icon_disabled_color = token_color(tokens, "color-text-disabled");
	active_theme = name;
	active_is_flat = true;
	return {};
}

QString apply_from_settings() {
	QSettings settings;
	return apply(settings.value("theme", "Dark").toString());
}

QIcon icon(const QString& name) {
	const auto svg = read_file("data/icons/flat/" + name + ".svg");
	if (!svg) {
		return {};
	}

	const auto render = [&](const QColor& color, const int size) {
		QByteArray data = *svg;
		data.replace("currentColor", color.name().toLatin1());
		QSvgRenderer renderer(data);
		QPixmap pixmap(size * 2, size * 2);
		pixmap.setDevicePixelRatio(2.0);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		renderer.render(&painter, QRectF(0, 0, size, size));
		return pixmap;
	};

	QIcon result;
	for (const int size : { 16, 20 }) {
		result.addPixmap(render(icon_color, size), QIcon::Normal);
		result.addPixmap(render(icon_disabled_color, size), QIcon::Disabled);
	}
	return result;
}
}
