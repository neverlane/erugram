/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "core/plugins/plugin.h"

namespace Core::Plugins {

class Manager final {
public:
	Manager();
	~Manager();

	void start();

	[[nodiscard]] std::vector<PluginInfo> list() const;
	[[nodiscard]] rpl::producer<> changes() const;

	[[nodiscard]] QString create(const QString &name);
	[[nodiscard]] QString read(const QString &id) const;
	void write(const QString &id, const QString &source);
	void remove(const QString &id);
	void setEnabled(const QString &id, bool enabled);
	void reload(const QString &id);

private:
	struct Entry;

	[[nodiscard]] QString directory() const;
	void ensureDirectory() const;
	[[nodiscard]] Entry *find(const QString &id);

	void scan();
	void load(Entry &entry);
	void unload(Entry &entry);
	void callHook(Entry &entry, const char *name);

	[[nodiscard]] QStringList readEnabledList() const;
	void writeEnabledList() const;

	std::vector<std::unique_ptr<Entry>> _entries;
	rpl::event_stream<> _changes;
	bool _started = false;

};

} // namespace Core::Plugins
