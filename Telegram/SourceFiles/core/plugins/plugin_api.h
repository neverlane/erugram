/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace sol {
class state;
} // namespace sol

namespace Core::Plugins {

struct ApiContext {
	QString id;
	QString dataDir;
};

void RegisterApi(sol::state &lua, const ApiContext &context);

} // namespace Core::Plugins
