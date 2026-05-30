/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/plugins/plugin_manager.h"

#include "base/debug_log.h"
#include "ui/toast/toast.h"
#include "settings.h"

#include <sol/sol.hpp>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Core::Plugins {
namespace {

constexpr auto kEnabledListFile = "plugins.json";

[[nodiscard]] QString Slugify(const QString &name) {
	auto result = QString();
	result.reserve(name.size());
	for (const auto ch : name) {
		if (ch.isLetterOrNumber()) {
			result.append(ch.toLower());
		} else if (!result.isEmpty() && !result.endsWith('_')) {
			result.append('_');
		}
	}
	while (result.endsWith('_')) {
		result.chop(1);
	}
	return result.isEmpty() ? u"plugin"_q : result;
}

[[nodiscard]] PluginInfo ParseMetadata(
		const QString &id,
		const QString &path,
		const QString &source) {
	auto info = PluginInfo();
	info.id = id;
	info.path = path;
	info.name = id;
	for (const auto &line : source.split('\n')) {
		const auto trimmed = line.trimmed();
		if (trimmed.isEmpty()) {
			continue;
		} else if (!trimmed.startsWith(u"--"_q)) {
			break;
		}
		const auto comment = trimmed.mid(2).trimmed();
		if (!comment.startsWith('@')) {
			continue;
		}
		const auto space = comment.indexOf(' ');
		if (space < 0) {
			continue;
		}
		const auto key = comment.mid(1, space - 1);
		const auto value = comment.mid(space + 1).trimmed();
		if (key == u"name"_q) {
			info.name = value;
		} else if (key == u"version"_q) {
			info.version = value;
		} else if (key == u"author"_q) {
			info.author = value;
		} else if (key == u"description"_q) {
			info.description = value;
		}
	}
	return info;
}

} // namespace

struct Manager::Entry {
	PluginInfo info;
	std::unique_ptr<sol::state> state;
};

Manager::Manager() = default;

Manager::~Manager() {
	for (auto &entry : _entries) {
		unload(*entry);
	}
}

QString Manager::directory() const {
	return cWorkingDir() + u"plugins/"_q;
}

void Manager::ensureDirectory() const {
	QDir().mkpath(directory());
}

Manager::Entry *Manager::find(const QString &id) {
	for (auto &entry : _entries) {
		if (entry->info.id == id) {
			return entry.get();
		}
	}
	return nullptr;
}

void Manager::start() {
	if (_started) {
		return;
	}
	_started = true;
	scan();
	for (auto &entry : _entries) {
		if (entry->info.enabled) {
			load(*entry);
		}
	}
	_changes.fire({});
}

std::vector<PluginInfo> Manager::list() const {
	auto result = std::vector<PluginInfo>();
	result.reserve(_entries.size());
	for (const auto &entry : _entries) {
		result.push_back(entry->info);
	}
	return result;
}

rpl::producer<> Manager::changes() const {
	return _changes.events();
}

void Manager::scan() {
	ensureDirectory();
	const auto enabled = readEnabledList();
	const auto files = QDir(directory()).entryInfoList(
		QStringList{ u"*.lua"_q },
		QDir::Files,
		QDir::Name);
	for (const auto &file : files) {
		const auto id = file.completeBaseName();
		if (find(id)) {
			continue;
		}
		auto content = QString();
		if (auto f = QFile(file.absoluteFilePath())
			; f.open(QIODevice::ReadOnly)) {
			content = QString::fromUtf8(f.readAll());
		}
		auto entry = std::make_unique<Entry>();
		entry->info = ParseMetadata(id, file.absoluteFilePath(), content);
		entry->info.enabled = enabled.contains(id);
		_entries.push_back(std::move(entry));
	}
}

void Manager::load(Entry &entry) {
	unload(entry);

	auto state = std::make_unique<sol::state>();
	state->open_libraries(
		sol::lib::base,
		sol::lib::string,
		sol::lib::table,
		sol::lib::math,
		sol::lib::utf8,
		sol::lib::coroutine);

	const auto id = entry.info.id;
	auto eru = state->create_named_table("eru");
	eru.set_function("log", [id](
			sol::this_state ts,
			sol::variadic_args args) {
		auto lua = sol::state_view(ts);
		sol::protected_function tostring = lua["tostring"];
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

	const auto result = state->safe_script_file(
		entry.info.path.toStdString(),
		sol::script_pass_on_error);
	if (!result.valid()) {
		const sol::error err = result;
		entry.info.failed = true;
		entry.info.error = QString::fromUtf8(err.what());
		LOG(("Plugin Error: failed to load '%1': %2").arg(
			entry.info.id,
			entry.info.error));
		return;
	}
	entry.info.failed = false;
	entry.info.error = QString();
	entry.state = std::move(state);
	callHook(entry, "on_load");
}

void Manager::unload(Entry &entry) {
	if (entry.state) {
		callHook(entry, "on_unload");
		entry.state = nullptr;
	}
	entry.info.failed = false;
	entry.info.error = QString();
}

void Manager::callHook(Entry &entry, const char *name) {
	if (!entry.state) {
		return;
	}
	auto &lua = *entry.state;
	sol::optional<sol::protected_function> handler = lua["eru"][name];
	if (!handler) {
		return;
	}
	const auto result = (*handler)();
	if (!result.valid()) {
		const sol::error err = result;
		const auto text = QString::fromUtf8(err.what());
		LOG(("Plugin Error: hook '%1' in '%2': %3").arg(
			QString::fromUtf8(name),
			entry.info.id,
			text));
		entry.info.error = text;
		_changes.fire({});
	}
}

QString Manager::create(const QString &name) {
	ensureDirectory();
	const auto base = Slugify(name);
	auto id = base;
	auto index = 1;
	while (find(id) || QFile::exists(directory() + id + u".lua"_q)) {
		id = base + u"_%1"_q.arg(++index);
	}
	const auto path = directory() + id + u".lua"_q;
	const auto source = u"-- @name %1\n"
		"-- @version 1.0.0\n"
		"-- @author \n"
		"-- @description \n"
		"\n"
		"eru.on_load = function()\n"
		"\teru.log(\"%1 loaded\")\n"
		"end\n"_q.arg(name);
	if (auto f = QFile(path); f.open(QIODevice::WriteOnly)) {
		f.write(source.toUtf8());
	}
	auto entry = std::make_unique<Entry>();
	entry->info = ParseMetadata(id, path, source);
	_entries.push_back(std::move(entry));
	_changes.fire({});
	return id;
}

QString Manager::read(const QString &id) const {
	for (const auto &entry : _entries) {
		if (entry->info.id == id) {
			if (auto f = QFile(entry->info.path)
				; f.open(QIODevice::ReadOnly)) {
				return QString::fromUtf8(f.readAll());
			}
			break;
		}
	}
	return QString();
}

void Manager::write(const QString &id, const QString &source) {
	const auto entry = find(id);
	if (!entry) {
		return;
	}
	if (auto f = QFile(entry->info.path); f.open(QIODevice::WriteOnly)) {
		f.write(source.toUtf8());
	}
	const auto enabled = entry->info.enabled;
	entry->info = ParseMetadata(id, entry->info.path, source);
	entry->info.enabled = enabled;
	if (enabled) {
		load(*entry);
	}
	_changes.fire({});
}

void Manager::remove(const QString &id) {
	for (auto i = _entries.begin(); i != _entries.end(); ++i) {
		if ((*i)->info.id != id) {
			continue;
		}
		unload(**i);
		QFile::remove((*i)->info.path);
		_entries.erase(i);
		writeEnabledList();
		_changes.fire({});
		return;
	}
}

void Manager::setEnabled(const QString &id, bool enabled) {
	const auto entry = find(id);
	if (!entry || entry->info.enabled == enabled) {
		return;
	}
	entry->info.enabled = enabled;
	if (enabled) {
		load(*entry);
	} else {
		unload(*entry);
	}
	writeEnabledList();
	_changes.fire({});
}

void Manager::reload(const QString &id) {
	const auto entry = find(id);
	if (entry && entry->info.enabled) {
		load(*entry);
		_changes.fire({});
	}
}

QStringList Manager::readEnabledList() const {
	auto f = QFile(directory() + QString::fromUtf8(kEnabledListFile));
	if (!f.open(QIODevice::ReadOnly)) {
		return {};
	}
	const auto document = QJsonDocument::fromJson(f.readAll());
	auto result = QStringList();
	for (const auto value : document.object().value(u"enabled"_q).toArray()) {
		result.push_back(value.toString());
	}
	return result;
}

void Manager::writeEnabledList() const {
	auto array = QJsonArray();
	for (const auto &entry : _entries) {
		if (entry->info.enabled) {
			array.push_back(entry->info.id);
		}
	}
	auto object = QJsonObject();
	object.insert(u"enabled"_q, array);
	auto f = QFile(directory() + QString::fromUtf8(kEnabledListFile));
	if (f.open(QIODevice::WriteOnly)) {
		f.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
	}
}

} // namespace Core::Plugins
