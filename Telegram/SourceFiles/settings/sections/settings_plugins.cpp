/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/sections/settings_plugins.h"

#include "core/application.h"
#include "core/plugins/plugin.h"
#include "core/plugins/plugin_manager.h"
#include "lang/lang_keys.h"
#include "settings/settings_common.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

constexpr auto kNewPluginBody = R"(eru.on_load = function()
	eru.toast("Hello from a plugin!")
end
)";

[[nodiscard]] QString BuildHeader(const QString &name) {
	return u"-- @name %1\n"
		"-- @version 1.0.0\n"
		"-- @author\n"
		"-- @description\n"
		"\n"_q.arg(name);
}

[[nodiscard]] auto FindInfo(const QString &id)
-> std::optional<Core::Plugins::PluginInfo> {
	for (const auto &info : Core::App().plugins().list()) {
		if (info.id == id) {
			return info;
		}
	}
	return std::nullopt;
}

void EditPluginBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		QString id) {
	const auto isNew = id.isEmpty();
	const auto info = isNew ? std::nullopt : FindInfo(id);

	box->setTitle(isNew
		? tr::lng_settings_plugins_new()
		: tr::lng_settings_plugins_edit());

	if (info) {
		const auto toggle = box->verticalLayout()->add(
			object_ptr<Ui::SettingsButton>(
				box->verticalLayout(),
				tr::lng_settings_plugins_enable(),
				st::settingsButtonNoIcon));
		toggle->toggleOn(rpl::single(info->enabled));
		toggle->toggledChanges(
		) | rpl::on_next([=](bool enabled) {
			Core::App().plugins().setEnabled(id, enabled);
		}, toggle->lifetime());
	}

	const auto nameField = isNew
		? box->addRow(object_ptr<Ui::InputField>(
			box.get(),
			st::defaultInputField,
			tr::lng_settings_plugins_name()))
		: nullptr;

	const auto initialCode = isNew
		? QString::fromUtf8(kNewPluginBody)
		: Core::App().plugins().read(id);
	const auto codeField = box->addRow(object_ptr<Ui::InputField>(
		box.get(),
		st::settingsChatLinkField,
		Ui::InputField::Mode::MultiLine,
		tr::lng_settings_plugins_code(),
		initialCode));

	box->setFocusCallback([=] {
		if (nameField) {
			nameField->setFocusFast();
		} else {
			codeField->setFocusFast();
		}
	});

	if (info && info->failed && !info->error.isEmpty()) {
		Ui::AddSkip(box->verticalLayout());
		Ui::AddDividerText(
			box->verticalLayout(),
			rpl::single(info->error));
	}

	box->addButton(tr::lng_settings_plugins_save(), [=] {
		auto &plugins = Core::App().plugins();
		auto pluginId = id;
		auto source = codeField->getLastText();
		if (isNew) {
			const auto name = nameField->getLastText().trimmed();
			if (name.isEmpty()) {
				nameField->showError();
				return;
			}
			pluginId = plugins.create(name);
			source = BuildHeader(name) + source;
		}
		plugins.write(pluginId, source);
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});

	if (!isNew) {
		box->addLeftButton(tr::lng_settings_plugins_delete(), [=] {
			const auto name = info ? info->name : id;
			const auto removed = [=](Fn<void()> close) {
				Core::App().plugins().remove(id);
				close();
				box->closeBox();
			};
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_settings_plugins_delete_sure(
					tr::now,
					lt_name,
					name),
				.confirmed = removed,
				.confirmText = tr::lng_settings_plugins_delete(),
				.confirmStyle = &st::attentionBoxButton,
			}));
		});
	}
}

void SetupPlugins(
		not_null<Window::SessionController*> controller,
		not_null<Ui::VerticalLayout*> container) {
	Ui::AddSkip(container);
	Ui::AddDividerText(container, tr::lng_settings_plugins_about());
	Ui::AddSkip(container);
	Ui::AddSubsectionTitle(container, tr::lng_settings_plugins_installed());

	const auto list = container->add(
		object_ptr<Ui::VerticalLayout>(container));

	const auto rebuild = [=] {
		list->clear();
		const auto all = Core::App().plugins().list();
		if (all.empty()) {
			Ui::AddDividerText(list, tr::lng_settings_plugins_empty());
		}
		for (const auto &info : all) {
			const auto id = info.id;
			const auto title = info.version.isEmpty()
				? info.name
				: (info.name + ' ' + info.version);
			const auto label = info.failed
				? u"⚠"_q
				: (info.enabled
					? tr::lng_settings_plugins_on(tr::now)
					: tr::lng_settings_plugins_off(tr::now));
			const auto button = AddButtonWithLabel(
				list,
				rpl::single(title),
				rpl::single(label),
				st::settingsButtonNoIcon);
			button->setClickedCallback([=] {
				controller->show(Box(EditPluginBox, controller, id));
			});
		}
	};
	rebuild();
	Core::App().plugins().changes(
	) | rpl::on_next(rebuild, container->lifetime());

	Ui::AddSkip(container);
	const auto add = AddButtonWithIcon(
		container,
		tr::lng_settings_plugins_add(),
		st::settingsButton,
		{ &st::menuIconBotCommands });
	add->setClickedCallback([=] {
		controller->show(Box(EditPluginBox, controller, QString()));
	});
	Ui::AddSkip(container);
}

} // namespace

Plugins::Plugins(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> Plugins::title() {
	return tr::lng_settings_plugins();
}

void Plugins::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	SetupPlugins(controller(), content);

	Ui::ResizeFitChild(this, content);
}

Type PluginsId() {
	return Plugins::Id();
}

} // namespace Settings
