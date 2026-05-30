/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Core::Plugins {

struct PluginInfo {
	QString id;
	QString path;
	QString name;
	QString version;
	QString author;
	QString description;
	bool enabled = false;
	bool failed = false;
	QString error;
};

} // namespace Core::Plugins
