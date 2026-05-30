/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/plugins/plugin_api.h"

#include "base/debug_log.h"
#include "core/version.h"
#include "ui/toast/toast.h"

#include <sol/sol.hpp>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Core::Plugins {
namespace {

[[nodiscard]] QString SafeName(const std::string &name) {
	return QFileInfo(QString::fromUtf8(name.c_str())).fileName();
}

[[nodiscard]] QString StorePath(const ApiContext &context) {
	return context.dataDir + u"store.json"_q;
}

[[nodiscard]] QJsonObject ReadStore(const ApiContext &context) {
	auto f = QFile(StorePath(context));
	if (!f.open(QIODevice::ReadOnly)) {
		return {};
	}
	return QJsonDocument::fromJson(f.readAll()).object();
}

void WriteStore(const ApiContext &context, const QJsonObject &object) {
	QDir().mkpath(context.dataDir);
	auto f = QFile(StorePath(context));
	if (f.open(QIODevice::WriteOnly)) {
		f.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
	}
}

} // namespace

void RegisterApi(sol::state &lua, const ApiContext &context) {
	const auto id = context.id;
	const auto dataDir = context.dataDir;

	auto eru = lua.create_named_table("eru");

	eru.set_function("log", [id](
			sol::this_state ts,
			sol::variadic_args args) {
		auto view = sol::state_view(ts);
		sol::protected_function tostring = view["tostring"];
		auto parts = QStringList();
		for (auto &&value : args) {
			const std::string text = tostring(value);
			parts.push_back(QString::fromUtf8(text.c_str()));
		}
		LOG(("Plugin '%1': %2").arg(id, parts.join(' ')));
	});

	eru.set_function("toast", [](const std::string &text) {
		Ui::Toast::Show(QString::fromUtf8(text.c_str()));
	});

	eru.set_function("app_version", [] {
		return std::string(AppVersionStr);
	});

	eru.set_function("store_get", [context](const std::string &key)
			-> sol::optional<std::string> {
		const auto object = ReadStore(context);
		const auto value = object.value(QString::fromUtf8(key.c_str()));
		if (value.isUndefined() || value.isNull()) {
			return std::nullopt;
		}
		return value.toString().toStdString();
	});

	eru.set_function("store_set", [context](
			const std::string &key,
			const std::string &value) {
		auto object = ReadStore(context);
		object.insert(
			QString::fromUtf8(key.c_str()),
			QString::fromUtf8(value.c_str()));
		WriteStore(context, object);
	});

	eru.set_function("read_file", [dataDir](const std::string &name)
			-> sol::optional<std::string> {
		auto f = QFile(dataDir + SafeName(name));
		if (!f.open(QIODevice::ReadOnly)) {
			return std::nullopt;
		}
		return std::string(f.readAll().constData());
	});

	eru.set_function("write_file", [dataDir](
			const std::string &name,
			const std::string &content) {
		QDir().mkpath(dataDir);
		auto f = QFile(dataDir + SafeName(name));
		if (!f.open(QIODevice::WriteOnly)) {
			return false;
		}
		f.write(QByteArray::fromStdString(content));
		return true;
	});
}

} // namespace Core::Plugins
