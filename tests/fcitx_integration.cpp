#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <testfrontend_public.h>
#include "smarttype/fcitx_safety.hpp"
#include "smarttype/personal_store.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

using namespace fcitx;

namespace {
void send_alt_shift(AddonInstance* frontend, const ICUUID& uuid) {
    frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_Alt_L), false);
    frontend->call<ITestFrontend::keyEvent>(uuid, Key("Alt+Shift_L"), false);
    frontend->call<ITestFrontend::keyEvent>(uuid, Key("Alt+Shift_L"), true);
    frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_Alt_L), true);
}
}

int main() {
    const auto isolated_env_present = [](const char* name) {
        const char* value = std::getenv(name);
        return value && *value;
    };
    const char* integration_test = std::getenv("SMARTTYPE_INTEGRATION_TEST");
    if (!integration_test || std::string_view(integration_test) != "1" ||
        !isolated_env_present("XDG_DATA_HOME") ||
        !isolated_env_present("XDG_CONFIG_HOME") ||
        !isolated_env_present("XDG_STATE_HOME")) {
        std::cerr << "FAIL: fcitx integration test requires SMARTTYPE_INTEGRATION_TEST=1 "
                     "and isolated XDG_DATA_HOME/XDG_CONFIG_HOME/XDG_STATE_HOME\n";
        return 2;
    }

    std::thread global_server_thread;
    if (const char* data_home = std::getenv("XDG_DATA_HOME")) {
        std::filesystem::remove_all(std::filesystem::path(data_home) / "smarttype");
    }
    if (const char* config_home = std::getenv("XDG_CONFIG_HOME")) {
        std::filesystem::remove_all(std::filesystem::path(config_home) / "fcitx5");
        std::filesystem::create_directories(std::filesystem::path(config_home) / "fcitx5");
        std::ofstream config(std::filesystem::path(config_home) / "fcitx5/config");
        config << "[Hotkey]\nModifierOnlyKeyTimeout=-1\n\n"
                  "[Hotkey/TriggerKeys]\n0=Alt+Shift_L\n1=Shift+Alt_L\n\n"
                  "[Hotkey/AltTriggerKeys]\n\n"
                  "[Behavior]\nShareInputState=All\n";
    }
    char arg0[] = "smarttype-fcitx-test";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=smarttype,testfrontend";
    char* argv[] = {arg0, arg1, arg2};
    Instance instance(3, argv);
    std::unique_ptr<fcitx::EventSourceTime> delayed_exit_timer;
    instance.addonManager().registerDefaultLoader(nullptr);
    // `Instance::eventDispatcher()` is absent in Ubuntu 24.04's Fcitx 5.1.7.
    // A local dispatcher works with both the older and current Fcitx APIs.
    fcitx::EventDispatcher dispatcher;
    dispatcher.attach(&instance.eventLoop());

    dispatcher.schedule(
        [&instance, &global_server_thread, &delayed_exit_timer]() {
        smarttype::PersonalStore init_store;
        init_store.set_setting("external_ui", false);
        init_store.set_setting("inline_correction_flash", false);

        auto group = instance.inputMethodManager().currentGroup();
        group.inputMethodList().clear();
        group.inputMethodList().push_back(InputMethodGroupItem("smarttype-us"));
        group.inputMethodList().push_back(InputMethodGroupItem("smarttype"));
        instance.inputMethodManager().setGroup(std::move(group));

        auto* frontend = instance.addonManager().addon("testfrontend", true);
        frontend->call<ITestFrontend::pushCommitExpectation>("Сегодня ");
        frontend->call<ITestFrontend::pushCommitExpectation>("Севодня ");
        frontend->call<ITestFrontend::pushCommitExpectation>("сегодня, ");
        frontend->call<ITestFrontend::pushCommitExpectation>("телевизор ");
        frontend->call<ITestFrontend::pushCommitExpectation>(", ");
        frontend->call<ITestFrontend::pushCommitExpectation>("1");
        frontend->call<ITestFrontend::pushCommitExpectation>("0");
        frontend->call<ITestFrontend::pushCommitExpectation>(" ");
        frontend->call<ITestFrontend::pushCommitExpectation>("% ");
        frontend->call<ITestFrontend::pushCommitExpectation>(" ");
        frontend->call<ITestFrontend::pushCommitExpectation>("$");
        frontend->call<ITestFrontend::pushCommitExpectation>("привет? ");
        frontend->call<ITestFrontend::pushCommitExpectation>("Как! ");
        frontend->call<ITestFrontend::pushCommitExpectation>("Да! ");
        frontend->call<ITestFrontend::pushCommitExpectation>("! ");
        frontend->call<ITestFrontend::pushCommitExpectation>("Нет ");
        frontend->call<ITestFrontend::pushCommitExpectation>("привет, ");
        frontend->call<ITestFrontend::pushCommitExpectation>("как ");
        frontend->call<ITestFrontend::pushCommitExpectation>("да? ");
        frontend->call<ITestFrontend::pushCommitExpectation>("«");
        frontend->call<ITestFrontend::pushCommitExpectation>("<");
        frontend->call<ITestFrontend::pushCommitExpectation>("«");
        frontend->call<ITestFrontend::pushCommitExpectation>(">");
        frontend->call<ITestFrontend::pushCommitExpectation>("»");
        frontend->call<ITestFrontend::pushCommitExpectation>("—");
        frontend->call<ITestFrontend::pushCommitExpectation>("нет ");
        frontend->call<ITestFrontend::pushCommitExpectation>("да! ");
        frontend->call<ITestFrontend::pushCommitExpectation>("Нет ");
        frontend->call<ITestFrontend::pushCommitExpectation>("Happ ");
        frontend->call<ITestFrontend::pushCommitExpectation>("ашипка ");
        frontend->call<ITestFrontend::pushCommitExpectation>("ошибка ");
        frontend->call<ITestFrontend::pushCommitExpectation>("ошибка ");
        frontend->call<ITestFrontend::pushCommitExpectation>("Работать ");
        frontend->call<ITestFrontend::pushCommitExpectation>("теливзор ");
        frontend->call<ITestFrontend::pushCommitExpectation>("ПРИвет ");
        frontend->call<ITestFrontend::pushCommitExpectation>("ghjtrn ");
        frontend->call<ITestFrontend::pushCommitExpectation>("-- ");
        frontend->call<ITestFrontend::pushCommitExpectation>("телевизор ");
        frontend->call<ITestFrontend::pushCommitExpectation>(", ");
        frontend->call<ITestFrontend::pushCommitExpectation>("привет,");
        frontend->call<ITestFrontend::pushCommitExpectation>("привет! ");
        frontend->call<ITestFrontend::pushCommitExpectation>("как ");
        auto pushProtectedExpectations = [frontend](const std::string& head,
                                                    const std::string& tail) {
            frontend->call<ITestFrontend::pushCommitExpectation>(head);
            for (const char value : tail) {
                frontend->call<ITestFrontend::pushCommitExpectation>(std::string(1, value));
            }
        };
        pushProtectedExpectations("https:", "//example.com/test ");
        pushProtectedExpectations("user@", "example.com ");
        pushProtectedExpectations("C:", "\\Users\\Test ");
        frontend->call<ITestFrontend::pushCommitExpectation>("1");
        frontend->call<ITestFrontend::pushCommitExpectation>("9");
        frontend->call<ITestFrontend::pushCommitExpectation>("2");
        pushProtectedExpectations(".", "168.1.1 ");
        frontend->call<ITestFrontend::pushCommitExpectation>("т.");
        frontend->call<ITestFrontend::pushCommitExpectation>("д");
        frontend->call<ITestFrontend::pushCommitExpectation>(".");
        frontend->call<ITestFrontend::pushCommitExpectation>(" ");
        pushProtectedExpectations("д.", " ");
        frontend->call<ITestFrontend::pushCommitExpectation>("1");
        frontend->call<ITestFrontend::pushCommitExpectation>("5");
        frontend->call<ITestFrontend::pushCommitExpectation>(" ");
        pushProtectedExpectations("site.", "ru ");
        frontend->call<ITestFrontend::pushCommitExpectation>("1");
        pushProtectedExpectations(".", "5 ");
        frontend->call<ITestFrontend::pushCommitExpectation>("1");
        frontend->call<ITestFrontend::pushCommitExpectation>("0");
        frontend->call<ITestFrontend::pushCommitExpectation>(" ");
        frontend->call<ITestFrontend::pushCommitExpectation>("% ");
        frontend->call<ITestFrontend::pushCommitExpectation>("с");
        frontend->call<ITestFrontend::pushCommitExpectation>("севодня");
        frontend->call<ITestFrontend::pushCommitExpectation>("Севодня");
        frontend->call<ITestFrontend::pushCommitExpectation>(" ");
        frontend->call<ITestFrontend::pushCommitExpectation>("п");

        const auto uuid = frontend->call<ITestFrontend::createInputContext>("telegram-desktop");
        auto* context = instance.inputContextManager().findByUUID(uuid);
        CapabilityFlags flags = CapabilityFlag::Preedit;
        flags |= CapabilityFlag::SurroundingText;
        context->setCapabilityFlags(flags);
        instance.setCurrentInputMethod(context, "smarttype", false);

        const Key typo[] = {
            Key(FcitxKey_Cyrillic_es), Key(FcitxKey_Cyrillic_ie), Key(FcitxKey_Cyrillic_ve),
            Key(FcitxKey_Cyrillic_o), Key(FcitxKey_Cyrillic_de), Key(FcitxKey_Cyrillic_en),
            Key(FcitxKey_Cyrillic_ya)};
        for (const auto& key : typo) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_BackSpace), false);

        // Punctuation can also terminate and correct the word directly.
        for (const auto& key : typo) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_comma), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_BackSpace), false);

        // Punctuation typed after an auto-corrected word replaces the inserted
        // space and invalidates correction undo. Backspace then belongs to the
        // punctuation, not to the previous word.
        const Key television_typo[] = {
            Key(FcitxKey_Cyrillic_te), Key(FcitxKey_Cyrillic_ie),
            Key(FcitxKey_Cyrillic_el), Key(FcitxKey_Cyrillic_i),
            Key(FcitxKey_Cyrillic_ve), Key(FcitxKey_Cyrillic_ze),
            Key(FcitxKey_Cyrillic_o), Key(FcitxKey_Cyrillic_er)
        };
        for (const auto& key : television_typo) {
            frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        }
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_comma), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_BackSpace), false);

        // Percent attaches only to a preceding number. Dollar remains exactly
        // where the user typed it.
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("1"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("0"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_percent), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_BackSpace), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_dollar), false);

        const Key privet[] = {
            Key(FcitxKey_Cyrillic_pe), Key(FcitxKey_Cyrillic_er),
            Key(FcitxKey_Cyrillic_i), Key(FcitxKey_Cyrillic_ve),
            Key(FcitxKey_Cyrillic_ie), Key(FcitxKey_Cyrillic_te)
        };
        const Key kak[] = {
            Key(FcitxKey_Cyrillic_ka), Key(FcitxKey_Cyrillic_a),
            Key(FcitxKey_Cyrillic_ka)
        };
        const Key da[] = {Key(FcitxKey_Cyrillic_de), Key(FcitxKey_Cyrillic_a)};
        const Key net[] = {
            Key(FcitxKey_Cyrillic_en), Key(FcitxKey_Cyrillic_ie),
            Key(FcitxKey_Cyrillic_te)
        };
        for (const auto& key : privet) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_question), false);
        for (const auto& key : kak) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_exclam), false);
        for (const auto& key : da) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_exclam), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_exclam), false);
        for (const auto& key : net) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        for (const auto& key : privet) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_comma), false);
        for (const auto& key : kak) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        for (const auto& key : da) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_question), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_guillemotleft), false);
        for (const char* key : {"<", "<", ">", ">", "-", "-"}) {
            frontend->call<ITestFrontend::keyEvent>(uuid, Key(key), false);
        }
        for (const auto& key : net) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        for (const auto& key : da) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_exclam), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_Return), false);
        for (const auto& key : net) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);

        for (const char* key : {"H", "a", "p", "p"}) {
            frontend->call<ITestFrontend::keyEvent>(uuid, Key(key), false);
        }
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);

        // Manual correction tracking test
        const Key ashipka[] = {
            Key(FcitxKey_Cyrillic_a), Key(FcitxKey_Cyrillic_sha), Key(FcitxKey_Cyrillic_i),
            Key(FcitxKey_Cyrillic_pe), Key(FcitxKey_Cyrillic_ka), Key(FcitxKey_Cyrillic_a)
        };
        for (const auto& key : ashipka) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);

        for (int i = 0; i < 7; ++i) {
            frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_BackSpace), false);
        }

        const Key oshibka[] = {
            Key(FcitxKey_Cyrillic_o), Key(FcitxKey_Cyrillic_sha), Key(FcitxKey_Cyrillic_i),
            Key(FcitxKey_Cyrillic_be), Key(FcitxKey_Cyrillic_ka), Key(FcitxKey_Cyrillic_a)
        };
        for (const auto& key : oshibka) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);

        for (const auto& key : ashipka) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);

        // Shift is a text modifier, not a shortcut: capitalized typos must be
        // tracked and corrected as a whole word.
        const Key capitalized_typo[] = {
            Key(FcitxKey_Cyrillic_ER, KeyState::Shift), Key(FcitxKey_Cyrillic_a),
            Key(FcitxKey_Cyrillic_be), Key(FcitxKey_Cyrillic_o),
            Key(FcitxKey_Cyrillic_a), Key(FcitxKey_Cyrillic_te),
            Key(FcitxKey_Cyrillic_softsign)};
        for (const auto& key : capitalized_typo) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);

        auto sendAscii = [frontend, &uuid](const std::string& value) {
            for (const unsigned char byte : value) {
                frontend->call<ITestFrontend::keyEvent>(
                    uuid, Key(static_cast<KeySym>(byte)), false);
            }
        };
        smarttype::PersonalStore runtime_store;
        const auto learned_rules = runtime_store.rules();
        const bool learned_manual_pair =
            std::any_of(learned_rules.begin(), learned_rules.end(), [](const auto& rule) {
                return rule.typo == "ашипка" && rule.correction == "ошибка";
            });
        if (!learned_manual_pair) {
            std::cerr << "FAIL: manual correction did not persist learned rule "
                         "ашипка→ошибка\n";
            std::abort();
        }
        runtime_store.set_setting("autocorrect", false);
        for (const auto& key : television_typo) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        runtime_store.set_setting("autocorrect", true);

        runtime_store.set_setting("accidental_case", false);
        const Key mixed_case[] = {
            Key(FcitxKey_Cyrillic_PE, KeyState::Shift),
            Key(FcitxKey_Cyrillic_ER, KeyState::Shift),
            Key(FcitxKey_Cyrillic_I, KeyState::Shift),
            Key(FcitxKey_Cyrillic_ve), Key(FcitxKey_Cyrillic_ie), Key(FcitxKey_Cyrillic_te)
        };
        for (const auto& key : mixed_case) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        runtime_store.set_setting("accidental_case", true);

        runtime_store.set_setting("layout_correction", false);
        sendAscii("ghjtrn ");
        runtime_store.set_setting("layout_correction", true);

        runtime_store.set_setting("smart_punctuation", false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("-"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("-"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        runtime_store.set_setting("smart_punctuation", true);

        runtime_store.set_setting("auto_space_after_punctuation", false);
        for (const auto& key : television_typo) {
            frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        }
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_comma), false);
        for (const auto& key : privet) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_comma), false);
        runtime_store.set_setting("auto_space_after_punctuation", true);

        runtime_store.set_setting("sentence_capitalization", false);
        for (const auto& key : privet) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_exclam), false);
        for (const auto& key : kak) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        runtime_store.set_setting("sentence_capitalization", true);

        sendAscii("https://example.com/test ");
        sendAscii("user@example.com ");
        sendAscii("C:\\Users\\Test ");
        sendAscii("192.168.1.1 ");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_Cyrillic_te), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_period), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_Cyrillic_de), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_period), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_Cyrillic_de), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_period), false);
        sendAscii(" 15 site.ru 1.5 ");
        context->setCapabilityFlags(CapabilityFlag::Preedit);
        sendAscii("10 ");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_percent), false);

        // GNOME owns one physical US XKB group. Global pause must disable
        // correction/UI, not the selected SmartType RU/EN layout itself.
        runtime_store.set_setting("x11_normalize_layout", true);
        instance.setCurrentInputMethod(context, "smarttype", false);
        runtime_store.set_setting("enabled", false);
        if (!frontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("c"), false) ||
            !context->inputPanel().clientPreedit().empty() ||
            context->inputPanel().candidateList() != nullptr) {
            std::cerr << "FAIL: global pause disabled GNOME RU layout mapping\n";
            std::abort();
        }
        runtime_store.set_setting("enabled", true);
        runtime_store.set_setting("x11_normalize_layout", false);

        runtime_store.set_setting("suggestions", false);
        for (const auto& key : typo) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        if (context->inputPanel().candidateList()) std::abort();
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_Escape), false);
        runtime_store.set_setting("suggestions", true);

        // Escape and Shift+Enter belong to the application when SmartType has
        // no active composition or candidate panel.
        if (frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_Escape), false) ||
            frontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(FcitxKey_Return, KeyState::Shift), false)) {
            std::abort();
        }

        // Delete must remain an editing key for the client. It must not be
        // swallowed by the IME or committed as the control-character square
        // observed in Telegram.
        bool delete_forwarded = false;
        auto delete_watcher = instance.watchEvent(
            EventType::InputContextForwardKey, EventWatcherPhase::Default,
            [&](Event& raw_event) {
                auto& forwarded = static_cast<ForwardKeyEvent&>(raw_event);
                if (forwarded.inputContext() == context &&
                    forwarded.key().check(FcitxKey_Delete)) {
                    delete_forwarded = true;
                }
            });
        if (!frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_Delete), false) ||
            !delete_forwarded) {
            std::abort();
        }

        for (const auto& key : typo) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        for (const auto key : {FcitxKey_Left, FcitxKey_Right}) {
            if (!frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(key), false)) std::abort();
        }
        if (!frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_Up), false) ||
            context->inputPanel().candidateList() != nullptr ||
            !context->inputPanel().clientPreedit().empty()) {
            std::abort();
        }
        // Closing candidates with Up/Down must return editing keys to the
        // application; otherwise Telegram looks frozen until a mouse click.
        delete_forwarded = false;
        if (!frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_Delete), false) ||
            !delete_forwarded ||
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_Left), false) ||
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_Tab), false)) {
            std::abort();
        }

        // A candidate panel is anchored to the text caret. A mouse click moves
        // the client caret asynchronously, sometimes immediately after a key
        // event. The entire old composition must be cancelled, not merely the
        // visible candidate list. Otherwise the exact owner regression is:
        // type a typo on line 1, click line 3, press Space, and SmartType
        // commits the line-1 candidate at the new line-3 caret.
        for (const auto& key : typo) frontend->call<ITestFrontend::keyEvent>(uuid, key, false);
        if (context->inputPanel().candidateList() == nullptr) std::abort();
        context->setCursorRect(Rect(240, 120, 1, 20));
        if (context->inputPanel().candidateList() != nullptr ||
            !context->inputPanel().clientPreedit().empty()) {
            std::cerr << "FAIL: caret relocation left the old composition active\n";
            std::abort();
        }
        // SmartType owns delimiters even with an empty buffer. It must commit
        // exactly the new caret's Space, never the discarded word/candidate.
        frontend->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_space), false);
        delete_watcher.reset();
        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_Escape), false);

        frontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_Cyrillic_pe), false);
        if (frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_Home), false)) {
            std::abort();
        }
        if (!context->inputPanel().preedit().empty() ||
            !context->inputPanel().clientPreedit().empty()) {
            std::abort();
        }

        if (frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key("Super+c"), false) ||
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key("Super+v"), false)) {
            std::abort();
        }
        frontend->call<ITestFrontend::destroyInputContext>(uuid);

        runtime_store.set_setting("disable_in_terminals", true);
        const auto terminal_uuid = frontend->call<ITestFrontend::createInputContext>("org.kde.konsole");
        if (frontend->call<ITestFrontend::sendKeyEvent>(
                terminal_uuid, Key(FcitxKey_Cyrillic_es), false)) {
            std::abort();
        }
        frontend->call<ITestFrontend::destroyInputContext>(terminal_uuid);

        // Kali Xfce uses QTerminal, which does not advertise Fcitx's Terminal
        // capability. The program-name fallback must therefore enforce the
        // user-visible "Не исправлять в терминалах" setting.
        const auto qterminal_uuid =
            frontend->call<ITestFrontend::createInputContext>("qterminal");
        if (frontend->call<ITestFrontend::sendKeyEvent>(
                qterminal_uuid, Key(FcitxKey_Cyrillic_es), false)) {
            std::cerr << "FAIL: QTerminal bypassed disable_in_terminals\n";
            std::abort();
        }

        // Kali/X11 keeps its physical XKB group on US and uses SmartType's two
        // logical input methods for RU/EN. Terminal exclusion must therefore
        // preserve literal layout mapping without enabling correction/preedit.
        runtime_store.set_setting("x11_normalize_layout", true);
        auto* qterminal_context =
            instance.inputContextManager().findByUUID(qterminal_uuid);
        qterminal_context->setCapabilityFlags(CapabilityFlag::Preedit);
        instance.setCurrentInputMethod(qterminal_context, "smarttype", false);
        frontend->call<ITestFrontend::pushCommitExpectation>("с");
        if (!frontend->call<ITestFrontend::sendKeyEvent>(
                qterminal_uuid, Key("c"), false)) {
            std::cerr << "FAIL: QTerminal RU layout-only mapping was not handled\n";
            std::abort();
        }
        if (!qterminal_context->inputPanel().clientPreedit().empty() ||
            qterminal_context->inputPanel().candidateList() != nullptr) {
            std::cerr << "FAIL: QTerminal layout-only mode exposed SmartType UI\n";
            std::abort();
        }
        if (frontend->call<ITestFrontend::sendKeyEvent>(
                qterminal_uuid, Key("Control+c"), false)) {
            std::cerr << "FAIL: QTerminal layout-only mode swallowed Ctrl+C\n";
            std::abort();
        }
        runtime_store.set_setting("x11_normalize_layout", false);
        frontend->call<ITestFrontend::destroyInputContext>(qterminal_uuid);

        // Ubuntu's visible acceptance check uses GNOME Terminal. Cover its
        // actual process name as well: layout mapping remains available, but
        // correction UI/transactions and Ctrl shortcuts stay out of the way.
        const auto gnome_terminal_uuid =
            frontend->call<ITestFrontend::createInputContext>("gnome-terminal-server");
        auto* gnome_terminal_context =
            instance.inputContextManager().findByUUID(gnome_terminal_uuid);
        gnome_terminal_context->setCapabilityFlags(CapabilityFlag::Preedit);
        instance.setCurrentInputMethod(gnome_terminal_context, "smarttype", false);
        runtime_store.set_setting("x11_normalize_layout", true);
        frontend->call<ITestFrontend::pushCommitExpectation>("с");
        if (!frontend->call<ITestFrontend::sendKeyEvent>(
                gnome_terminal_uuid, Key("c"), false) ||
            !gnome_terminal_context->inputPanel().clientPreedit().empty() ||
            gnome_terminal_context->inputPanel().candidateList() != nullptr ||
            frontend->call<ITestFrontend::sendKeyEvent>(
                gnome_terminal_uuid, Key("Control+c"), false)) {
            std::cerr << "FAIL: GNOME Terminal exclusion is not stateless\n";
            std::abort();
        }
        runtime_store.set_setting("x11_normalize_layout", false);
        frontend->call<ITestFrontend::destroyInputContext>(gnome_terminal_uuid);

        // The tray's "pause in current application" is an app blacklist.
        // LibreOffice must keep stateless RU mapping while correction,
        // candidates and shortcuts are bypassed in that one context.
        const auto paused_writer_uuid =
            frontend->call<ITestFrontend::createInputContext>("soffice");
        auto* paused_writer_context =
            instance.inputContextManager().findByUUID(paused_writer_uuid);
        paused_writer_context->setCapabilityFlags(CapabilityFlag::Preedit);
        instance.setCurrentInputMethod(paused_writer_context, "smarttype", false);
        runtime_store.set_setting("x11_normalize_layout", true);
        runtime_store.blacklist_add("soffice");
        frontend->call<ITestFrontend::pushCommitExpectation>("с");
        if (!frontend->call<ITestFrontend::sendKeyEvent>(
                paused_writer_uuid, Key("c"), false) ||
            !paused_writer_context->inputPanel().clientPreedit().empty() ||
            paused_writer_context->inputPanel().candidateList() != nullptr ||
            frontend->call<ITestFrontend::sendKeyEvent>(
                paused_writer_uuid, Key("Control+c"), false)) {
            std::cerr << "FAIL: per-app pause disabled GNOME RU layout mapping\n";
            std::abort();
        }
        runtime_store.blacklist_remove("soffice");
        runtime_store.set_setting("x11_normalize_layout", false);
        frontend->call<ITestFrontend::destroyInputContext>(paused_writer_uuid);

        const auto password_uuid = frontend->call<ITestFrontend::createInputContext>("firefox");
        auto* password_context = instance.inputContextManager().findByUUID(password_uuid);
        password_context->setCapabilityFlags(CapabilityFlag::Password);
        if (frontend->call<ITestFrontend::sendKeyEvent>(
                password_uuid, Key(FcitxKey_Cyrillic_es), false)) {
            std::abort();
        }
        frontend->call<ITestFrontend::destroyInputContext>(password_uuid);

        // LibreOffice's generic VCL backend can expose raw XIM with no client
        // preedit. That exact combination must be fail-closed; rich XIM and
        // toolkit frontends remain usable.
        if (!smarttype::unsafe_raw_xim_context("xim", false) ||
            smarttype::unsafe_raw_xim_context("xim", true) ||
            smarttype::unsafe_raw_xim_context("dbus", false)) {
            std::cerr << "FAIL LibreOffice XIM: unsafe frontend predicate\n";
            std::abort();
        }

        const auto first_uuid = frontend->call<ITestFrontend::createInputContext>("first-app");
        auto* first_context = instance.inputContextManager().findByUUID(first_uuid);
        first_context->setCapabilityFlags(CapabilityFlag::Preedit);
        instance.setCurrentInputMethod(first_context, "smarttype-us", false);
        send_alt_shift(frontend, first_uuid);
        if (instance.inputMethod(first_context) != "smarttype") std::abort();

        const auto second_uuid = frontend->call<ITestFrontend::createInputContext>("second-app");
        auto* second_context = instance.inputContextManager().findByUUID(second_uuid);
        second_context->setCapabilityFlags(CapabilityFlag::Preedit);
        frontend->call<ITestFrontend::sendKeyEvent>(second_uuid, Key(FcitxKey_Alt_L), false);
        frontend->call<ITestFrontend::sendKeyEvent>(second_uuid, Key(FcitxKey_Alt_L), true);
        if (instance.inputMethod(second_context) != "smarttype") std::abort();
        send_alt_shift(frontend, second_uuid);
        if (instance.inputMethod(second_context) != "smarttype-us") std::abort();
        frontend->call<ITestFrontend::destroyInputContext>(first_uuid);
        frontend->call<ITestFrontend::destroyInputContext>(second_uuid);

        // Test layout toggle hotkey Ctrl+Shift+Space
        const auto test_uuid = frontend->call<ITestFrontend::createInputContext>("test-layout-toggle");
        auto* test_context = instance.inputContextManager().findByUUID(test_uuid);
        test_context->setCapabilityFlags(CapabilityFlag::Preedit);
        instance.setCurrentInputMethod(test_context, "smarttype", false); // Russian layout by default
        
        // Type keys that produce "привет" in Russian layout ("ghbdtn")
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_g), false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_h), false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_b), false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_d), false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_t), false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_n), false);
        
        if (test_context->inputPanel().clientPreedit().toString() != "Ghbdtn") {
            std::abort();
        }
        
        // Send Control+Shift+Space to toggle layout to RU
        frontend->call<ITestFrontend::sendKeyEvent>(test_uuid, Key("Control+Shift+space"), false);
        
        if (test_context->inputPanel().clientPreedit().toString() != "Привет") {
            std::abort();
        }
        
        // Send Control+Shift+Space again to toggle layout back to EN
        frontend->call<ITestFrontend::sendKeyEvent>(test_uuid, Key("Control+Shift+space"), false);
        
        if (test_context->inputPanel().clientPreedit().toString() != "Ghbdtn") {
            std::abort();
        }
        
        // Commit the buffer using Space so we start clean for the next tests
        frontend->call<ITestFrontend::pushCommitExpectation>("Ghbdtn ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        
        auto send_ascii = [frontend, &test_uuid](const std::string& value) {
            for (const unsigned char byte : value) {
                frontend->call<ITestFrontend::keyEvent>(
                    test_uuid, Key(static_cast<KeySym>(byte)), false);
            }
        };
        
        // Test layout mode: suggest (default)
        send_ascii("ghjtrn");
        
        // Candidate list should contain "проект" as the first candidate
        if (test_context->inputPanel().candidateList() == nullptr ||
            test_context->inputPanel().candidateList()->candidate(0).text().toString() != "проект") {
            std::abort();
        }
        
        // Press Space -> should commit "ghjtrn " (suggest-only does not auto-correct)
        frontend->call<ITestFrontend::pushCommitExpectation>("ghjtrn ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        
        // Test layout mode: auto
        runtime_store.set_string_setting("layout_mode", "auto");
        
        // Type "ghjtrn"
        send_ascii("ghjtrn");
        
        // Press Space -> should autocorrect to "проект "
        frontend->call<ITestFrontend::pushCommitExpectation>("проект ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        
        // Test layout mode auto safety: 2-letter EN→RU "gh" -> "пи" stays suggest-only
        send_ascii("gh");
        // Press Space -> should commit "gh " (no auto-correct for EN→RU len=2)
        frontend->call<ITestFrontend::pushCommitExpectation>("gh ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);

        // RU→EN 2-letter layout auto: "вщ" → "do " (unblocks "вщ нщг" → "do you")
        instance.setCurrentInputMethod(test_context, "smarttype", false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_ve), false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_shcha), false);
        frontend->call<ITestFrontend::pushCommitExpectation>("do ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        if (instance.inputMethod(test_context) != "smarttype-us") {
            std::cerr << "FAIL: expected IM smarttype-us after вщ→do layout auto\n";
            std::abort();
        }
        // Restore RU for subsequent tests that send Cyrillic keysyms.
        instance.setCurrentInputMethod(test_context, "smarttype", false);
        
        // ST-026b: 3-letter EN→RU with a real dictionary target auto-commits even
        // without Cyrillic context ("rfr"→"как", "xnj"→"что"). Weak non-dict
        // targets remain suggest-only (see len==3 branch in finish_word).
        send_ascii("rfr");
        frontend->call<ITestFrontend::pushCommitExpectation>("как ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        
        // Same 3-letter layout still works with Cyrillic context present.
        send_ascii("ghbdtn"); // will autocorrect to "привет " because layout_mode is auto and len >= 4
        frontend->call<ITestFrontend::pushCommitExpectation>("привет ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        
        send_ascii("rfr");
        frontend->call<ITestFrontend::pushCommitExpectation>("как ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);

        // Test layout mode auto with uppercase input: "GHBDTN" -> "ПРИВЕТ "
        send_ascii("GHBDTN");
        frontend->call<ITestFrontend::pushCommitExpectation>("ПРИВЕТ ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key("Shift+space"), false);
        
        // Test automatic layout switching:
        // Currently, active layout is "smarttype" (Russian)
        if (instance.inputMethod(test_context) != "smarttype") {
            std::abort();
        }
        
        // Type keys that produce "hello" in Russian layout ("руддщ")
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_er), false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_u), false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_de), false);
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_de), false);
        if (instance.inputMethod(test_context) != "smarttype") {
            std::cerr << "FAIL ST-041: RU physical IM changed before client preedit commit\n";
            std::abort();
        }
        // The physical IM stays RU until Space; queued source-layout keys are
        // mapped through the pending EN target.
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_shcha), false);
        
        // Press Space -> should autocorrect to "hello " and switch input method to "smarttype-us"
        frontend->call<ITestFrontend::pushCommitExpectation>("hello ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        
        if (instance.inputMethod(test_context) != "smarttype-us") {
            std::abort();
        }
        
        {
            // ST-041: translate client preedit immediately, but do not change the
            // physical IM until the word is committed and preedit is empty.
            int switch_count = 0;
            std::vector<std::string> preedit_at_switch;
            std::string last_preedit_update = "<none>";
            auto preedit_watcher = instance.watchEvent(
                EventType::InputContextUpdatePreedit, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& preedit_event = static_cast<InputContextEvent&>(raw_event);
                    if (preedit_event.inputContext() == test_context) {
                        last_preedit_update =
                            test_context->inputPanel().clientPreedit().toString();
                    }
                });
            auto switch_watcher = instance.watchEvent(
                EventType::InputContextSwitchInputMethod, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& switch_event =
                        static_cast<InputContextSwitchInputMethodEvent&>(raw_event);
                    if (switch_event.inputContext() != test_context) {
                        return;
                    }
                    ++switch_count;
                    preedit_at_switch.push_back(last_preedit_update);
                });

            // Also fail if client preedit is briefly cleared mid-composition
            // (the live Qt failure mode that produced "ghbdет" / "веет").
            int empty_preedit_while_composing = 0;
            bool watch_nonempty_preedit = false;
            auto empty_preedit_watcher = instance.watchEvent(
                EventType::InputContextUpdatePreedit, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& preedit_event = static_cast<InputContextEvent&>(raw_event);
                    if (preedit_event.inputContext() != test_context) {
                        return;
                    }
                    const std::string pe =
                        test_context->inputPanel().clientPreedit().toString();
                    if (watch_nonempty_preedit && pe.empty()) {
                        ++empty_preedit_while_composing;
                    }
                    if (!pe.empty()) {
                        watch_nonempty_preedit = true;
                    }
                    last_preedit_update = pe;
                });

            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_g), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_h), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_b), false);
            {
                const std::string pe =
                    test_context->inputPanel().clientPreedit().toString();
                if (pe != "при" && pe != "При") {
                    std::cerr << "FAIL ST-041: expected translated preedit при/При, got '"
                              << pe << "'\n";
                    std::abort();
                }
            }
            if (instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL ST-041: EN physical IM changed while preedit was non-empty\n";
                std::abort();
            }
            // Model a fast queue: d/t/n still arrive as source-layout Latin keysyms.
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_d), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_t), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_n), false);
            {
                const std::string pe =
                    test_context->inputPanel().clientPreedit().toString();
                if (pe != "привет" && pe != "Привет") {
                    std::cerr << "FAIL ST-041: queued source keys did not complete "
                                 "привет/Привет, got '"
                              << pe << "'\n";
                    std::abort();
                }
                if (empty_preedit_while_composing != 0) {
                    std::cerr << "FAIL ST-041: client preedit was cleared mid-composition ("
                              << empty_preedit_while_composing << " times)\n";
                    std::abort();
                }
                watch_nonempty_preedit = false;  // space may clear preedit legitimately
                frontend->call<ITestFrontend::pushCommitExpectation>(pe + " ");
            }
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);

            if (instance.inputMethod(test_context) != "smarttype" || switch_count != 1 ||
                preedit_at_switch.size() != 1 || !preedit_at_switch.front().empty()) {
                std::cerr << "FAIL ST-041: physical switch was not a single empty-preedit "
                             "word-boundary transaction\n";
                std::abort();
            }
        }

        // The KDE/XKB layout update can lag behind the Fcitx switch during a
        // burst, so the next words may still arrive as EN keysyms. They must be
        // corrected without bouncing the settled RU target back to EN.
        send_ascii("rfr");
        frontend->call<ITestFrontend::pushCommitExpectation>("как ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        send_ascii("ltkf");
        frontend->call<ITestFrontend::pushCommitExpectation>("дела ");
        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        if (instance.inputMethod(test_context) != "smarttype") {
            std::cerr << "FAIL ST-041: queued multiword burst bounced RU target back to EN\n";
            std::abort();
        }

        // Test proactive mid-word layout switching
        {
            instance.setCurrentInputMethod(test_context, "smarttype-us", false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_g), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_h), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_b), false);
            
            if (instance.inputMethod(test_context) != "smarttype-us" ||
                test_context->inputPanel().clientPreedit().toString() != "при") {
                std::cerr << "FAIL ST-041: proactive translation was not deferred" << std::endl;
                std::abort();
            }
            
            // Clean up buffer
            frontend->call<ITestFrontend::pushCommitExpectation>("при ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
            if (instance.inputMethod(test_context) != "smarttype") {
                std::cerr << "FAIL ST-041: deferred IM did not switch after commit\n";
                std::abort();
            }
        }

        // ST-019: immediate Backspace after proactive switch restores pre-switch snapshot
        {
            instance.setCurrentInputMethod(test_context, "smarttype-us", false);
            int switch_count = 0;
            auto switch_watcher = instance.watchEvent(
                EventType::InputContextSwitchInputMethod, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& switch_event =
                        static_cast<InputContextSwitchInputMethodEvent&>(raw_event);
                    if (switch_event.inputContext() == test_context) {
                        ++switch_count;
                    }
                });
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_g), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_h), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_b), false);

            if (instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL ST-041: physical IM changed before proactive undo" << std::endl;
                std::abort();
            }

            // Immediate BS → restore Latin snapshot + EN IM (not char-delete, not full retranslate mess)
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_BackSpace), false);

            if (instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL ST-019: immediate layout-switch undo did not restore EN IM!\n";
                std::abort();
            }
            const std::string after_undo = test_context->inputPanel().clientPreedit().toString();
            // Sentence cap may yield "Ghb"; accept either.
            if (after_undo != "ghb" && after_undo != "Ghb") {
                std::cerr << "FAIL ST-019: expected Latin snapshot ghb/Ghb, got '" << after_undo
                          << "'\n";
                std::abort();
            }

            frontend->call<ITestFrontend::pushCommitExpectation>(after_undo + " ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
            if (switch_count != 0) {
                std::cerr << "FAIL ST-041: proactive Backspace caused a transient IM switch\n";
                std::abort();
            }
        }

        // ST-019: after continuing to an unfinished post-switch token, BS
        // deletes one char (IM stays). Use "rfrz" -> "какя" so the extended
        // target is unambiguously not a complete dictionary word.
        {
            instance.setCurrentInputMethod(test_context, "smarttype-us", false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_r), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_f), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_r), false);
            if (instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL ST-041: physical IM changed before continue-type\n";
                std::abort();
            }
            // Extra queued EN key maps to RU and expires layout-undo episode.
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_z), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_BackSpace), false);

            if (instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL ST-019: char Backspace must keep deferred physical IM\n";
                std::abort();
            }
            const std::string after_bs = test_context->inputPanel().clientPreedit().toString();
            if (after_bs != "как" && after_bs != "Как") {
                std::cerr << "FAIL ST-019: expected one-char delete to leave как/Как, got '"
                          << after_bs << "'\n";
                std::abort();
            }

            frontend->call<ITestFrontend::pushCommitExpectation>(after_bs + " ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
            if (instance.inputMethod(test_context) != "smarttype") {
                std::cerr << "FAIL ST-041: continued word did not flush deferred IM\n";
                std::abort();
            }
        }

        // P0 clean-Fedora regression: the proactive snapshot used to stop at
        // the three-character trigger ("Ghb"). After the remaining physical
        // keys completed Ghbdtn -> Привет, Space + Backspace therefore restored
        // only "Ghb ". The committed undo transaction must retain every source
        // key, including the sentence-capitalized first letter and delimiter.
        {
            runtime_store.set_string_setting("layout_mode", "auto");
            runtime_store.set_setting("layout_correction", true);
            runtime_store.set_setting("sentence_capitalization", true);
            runtime_store.set_setting("inline_correction_flash", false);
            instance.setCurrentInputMethod(test_context, "smarttype-us", false);
            CapabilityFlags undo_flags = CapabilityFlag::Preedit;
            undo_flags |= CapabilityFlag::SurroundingText;
            test_context->setCapabilityFlags(undo_flags);

            const Key ghbdtn[] = {
                Key(FcitxKey_G), Key(FcitxKey_h), Key(FcitxKey_b),
                Key(FcitxKey_d), Key(FcitxKey_t), Key(FcitxKey_n)};

            // Owner follow-up: once the proactive result reaches the complete
            // dictionary word "Привет", Backspace before any delimiter must
            // undo the automatic layout replacement, not delete only "т".
            // Disable learning for this first undo so the second half can
            // independently exercise the existing Space + Backspace path.
            runtime_store.set_setting("learning", false);
            for (const auto& key : ghbdtn) {
                frontend->call<ITestFrontend::keyEvent>(test_uuid, key, false);
            }
            if (test_context->inputPanel().clientPreedit().toString() != "Привет") {
                std::cerr << "FAIL P0 undo: Ghbdtn did not proactively become Привет\n";
                std::abort();
            }
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_BackSpace), false);
            if (test_context->inputPanel().clientPreedit().toString() != "Ghbdtn" ||
                instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL P0 undo: pre-delimiter Backspace did not restore Ghbdtn\n";
                std::abort();
            }
            frontend->call<ITestFrontend::pushCommitExpectation>("Ghbdtn ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
            runtime_store.set_setting("learning", true);

            for (const auto& key : ghbdtn) {
                frontend->call<ITestFrontend::keyEvent>(test_uuid, key, false);
            }
            if (test_context->inputPanel().clientPreedit().toString() != "Привет") {
                std::cerr << "FAIL P0 undo: second Ghbdtn did not proactively become Привет\n";
                std::abort();
            }

            frontend->call<ITestFrontend::pushCommitExpectation>("Привет ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
            test_context->surroundingText().setText("Привет ", 7, 7);
            test_context->updateSurroundingText();
            frontend->call<ITestFrontend::pushCommitExpectation>("Ghbdtn ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_BackSpace), false);
            if (instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL P0 undo: original EN input method was not restored\n";
                std::abort();
            }
        }

        // Deleting an accepted proactive word all the way to an empty buffer
        // cancels its deferred target; a following empty Space must not leak it.
        {
            instance.setCurrentInputMethod(test_context, "smarttype-us", false);
            int switch_count = 0;
            auto switch_watcher = instance.watchEvent(
                EventType::InputContextSwitchInputMethod, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& switch_event =
                        static_cast<InputContextSwitchInputMethodEvent&>(raw_event);
                    if (switch_event.inputContext() == test_context) {
                        ++switch_count;
                    }
                });
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_r), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_f), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_r), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_f), false);
            for (int index = 0; index < 4; ++index) {
                frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_BackSpace),
                                                       false);
            }
            if (!test_context->inputPanel().clientPreedit().toString().empty() ||
                instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL ST-041: full buffer erase left deferred layout state\n";
                std::abort();
            }
            frontend->call<ITestFrontend::pushCommitExpectation>(" ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
            if (switch_count != 0 || instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL ST-041: empty word boundary flushed a deleted target\n";
                std::abort();
            }
        }

        // Test double-shift layout translation
        {
            CapabilityFlags flags = CapabilityFlag::Preedit;
            flags |= CapabilityFlag::SurroundingText;
            test_context->setCapabilityFlags(flags);
            test_context->surroundingText().setText("ghbdtn rfr ltkf", 15, 0);
            test_context->updateSurroundingText();
            
            frontend->call<ITestFrontend::pushCommitExpectation>("привет как дела");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Shift_L), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Shift_L), false);
        }
 
        // Test 5 simultaneous flash events
        {
            // Reset layout_mode to replace
            runtime_store.set_string_setting("layout_mode", "replace");
            runtime_store.set_setting("inline_correction_flash", true);
            runtime_store.set_setting("external_ui", true);
 
            // Set up an isolated unix socket server. Never reuse /tmp/smarttype-ui.sock:
            // deleting or killing a global UI process would make this test affect the
            // user's live desktop session.
            const char* old_runtime_env = std::getenv("XDG_RUNTIME_DIR");
            const bool had_runtime_env = old_runtime_env != nullptr;
            const std::string old_runtime = old_runtime_env ? old_runtime_env : "";
            const auto runtime_dir = std::filesystem::temp_directory_path() /
                                     ("smarttype-ui-test-" + std::to_string(getpid()));
            std::filesystem::remove_all(runtime_dir);
            std::filesystem::create_directories(runtime_dir);
            setenv("XDG_RUNTIME_DIR", runtime_dir.c_str(), 1);
            const auto sock_path = runtime_dir / "smarttype-ui.sock";
 
            int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (server_fd < 0) {
                std::cerr << "FAIL: Failed to create dummy UI socket\n";
                std::abort();
            }
            {
                struct sockaddr_un addr{};
                std::memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
 
                if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0 && listen(server_fd, 5) == 0) {
                    // Temporarily allow external UI in this test scope
                    setenv("SMARTTYPE_EXTERNAL_UI", "1", 1);
 
                    // Push expectations
                    for (int i = 0; i < 5; ++i) {
                        frontend->call<ITestFrontend::pushCommitExpectation>("сегодня ");
                    }
  
                    // Accept the connection and read the messages in a background thread.
                    // Must not block forever — otherwise the process cannot exit under ctest.
                    global_server_thread = std::thread([server_fd]() {
                        const int flags = fcntl(server_fd, F_GETFL, 0);
                        if (flags >= 0) {
                            fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
                        }
                        int client_fd = -1;
                        for (int attempt = 0; attempt < 300 && client_fd < 0; ++attempt) {
                            client_fd = accept(server_fd, nullptr, nullptr);
                            if (client_fd < 0) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                        }
                        if (client_fd < 0) {
                            std::cerr << "FAIL: External UI client did not connect to isolated socket\n";
                            std::abort();
                        }
                        timeval tv{};
                        tv.tv_sec = 2;
                        tv.tv_usec = 0;
                        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                        std::string accumulated_data;
                        char read_buf[1024];
                        int flash_count = 0;
                        while (true) {
                            std::memset(read_buf, 0, sizeof(read_buf));
                            ssize_t bytes_read = read(client_fd, read_buf, sizeof(read_buf) - 1);
                            if (bytes_read <= 0) {
                                break;
                            }
                            accumulated_data.append(read_buf, bytes_read);
                            flash_count = 0;
                            std::size_t pos = 0;
                            while ((pos = accumulated_data.find("\"type\":\"flash\"", pos)) !=
                                   std::string::npos) {
                                flash_count++;
                                pos += 14;
                            }
                            if (flash_count >= 5) {
                                break;
                            }
                        }
                        if (flash_count != 5) {
                            std::cerr << "FAIL: Expected 5 flash events, got " << flash_count
                                      << std::endl;
                            std::cerr << "Accumulated: " << accumulated_data << std::endl;
                            std::abort();
                        }
                        close(client_fd);
                    });
 
                    // We must trigger 5 corrections!
                    // Let's type "севодня" followed by space, 5 times.
                    for (int i = 0; i < 5; ++i) {
                        const Key word_keys[] = {
                            Key(FcitxKey_Cyrillic_es), Key(FcitxKey_Cyrillic_ie), Key(FcitxKey_Cyrillic_ve),
                            Key(FcitxKey_Cyrillic_o), Key(FcitxKey_Cyrillic_de), Key(FcitxKey_Cyrillic_en),
                            Key(FcitxKey_Cyrillic_ya)
                        };
                        for (const auto& key : word_keys) {
                            frontend->call<ITestFrontend::keyEvent>(test_uuid, key, false);
                        }
                        // Press Space -> commits "Сегодня " and triggers flash
                        frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
                    }
                    frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Return), false);
 
                    unsetenv("SMARTTYPE_EXTERNAL_UI");
                } else {
                    std::cerr << "FAIL: Failed to bind/listen on dummy UI socket path: " << sock_path << std::endl;
                    std::abort();
                }
                close(server_fd);
                std::filesystem::remove(sock_path);
            }
            if (global_server_thread.joinable()) {
                global_server_thread.join();
            }
            if (had_runtime_env) {
                setenv("XDG_RUNTIME_DIR", old_runtime.c_str(), 1);
            } else {
                unsetenv("XDG_RUNTIME_DIR");
            }
            std::filesystem::remove_all(runtime_dir);
            // Restore defaults left dirty by the flash test.
            runtime_store.set_setting("external_ui", false);
            runtime_store.set_setting("inline_correction_flash", false);
        }
 
        // Reset layout_mode to suggest (default)
        runtime_store.set_string_setting("layout_mode", "suggest");

        // ---- ST-017: layout state synchronization ----
        {
            // Avoid sentence-capitalization turning "ghb" into "Ghb" (breaks EN prefix checks).
            runtime_store.set_setting("sentence_capitalization", false);

            // 1) New IC with active smarttype-us gets the correct logical source.
            //    Proactive translation at length 3 proves it, while ST-041 keeps
            //    the physical IM unchanged until the word boundary.
            const auto st017_uuid =
                frontend->call<ITestFrontend::createInputContext>("st017-new-ic-us");
            auto* st017_ic = instance.inputContextManager().findByUUID(st017_uuid);
            st017_ic->setCapabilityFlags(CapabilityFlag::Preedit);
            // Force a full activate path smarttype → smarttype-us so logical IM
            // is synced via on_activate (not only global IM name).
            instance.setCurrentInputMethod(st017_ic, "smarttype", false);
            instance.setCurrentInputMethod(st017_ic, "smarttype-us", false);
            if (instance.inputMethod(st017_ic) != "smarttype-us") {
                std::cerr << "FAIL ST-017: new IC fcitx IM is not smarttype-us\n";
                std::abort();
            }
            // Clear any residual composition.
            frontend->call<ITestFrontend::keyEvent>(st017_uuid, Key(FcitxKey_Escape), false);
            // Use "rfr" → "как": reliable layout mismatch (not a valid EN prefix).
            frontend->call<ITestFrontend::keyEvent>(st017_uuid, Key(FcitxKey_r), false);
            frontend->call<ITestFrontend::keyEvent>(st017_uuid, Key(FcitxKey_f), false);
            frontend->call<ITestFrontend::keyEvent>(st017_uuid, Key(FcitxKey_r), false);
            if (instance.inputMethod(st017_ic) != "smarttype-us" ||
                st017_ic->inputPanel().clientPreedit().toString() != "как") {
                std::cerr << "FAIL ST-017/ST-041: new IC did not translate while deferring "
                             "physical IM; preedit='"
                          << st017_ic->inputPanel().clientPreedit().toString()
                          << "' im=" << instance.inputMethod(st017_ic) << "\n";
                std::abort();
            }
            // 2) reset/deactivate after programmatic switch must not wipe the word.
            const std::string preedit_after_switch =
                st017_ic->inputPanel().clientPreedit().toString();
            if (preedit_after_switch.empty()) {
                std::cerr << "FAIL ST-017: composing state wiped after programmatic "
                             "layout switch (reset/deactivate race)\n";
                std::abort();
            }
            // A queued source-layout Latin key is coerced through the pending RU target.
            // Leave preedit uncommitted (Escape would commit; Space may hunspell-fix).
            frontend->call<ITestFrontend::keyEvent>(st017_uuid, Key(FcitxKey_f), false);
            const std::string after_continue =
                st017_ic->inputPanel().clientPreedit().toString();
            if (after_continue != "кака" && after_continue != "Кака") {
                std::cerr << "FAIL ST-017: expected кака after continue-type, got '"
                          << after_continue << "'\n";
                std::abort();
            }

            // 3) New / re-activated IC overrides stale logical: set smarttype-us on a
            //    fresh IC — first keys behave as EN (proactive at len=3).
            const auto st017b_uuid =
                frontend->call<ITestFrontend::createInputContext>("st017-focus-resync");
            auto* st017b_ic = instance.inputContextManager().findByUUID(st017b_uuid);
            st017b_ic->setCapabilityFlags(CapabilityFlag::Preedit);
            instance.setCurrentInputMethod(st017b_ic, "smarttype", false);
            instance.setCurrentInputMethod(st017b_ic, "smarttype-us", false);
            if (instance.inputMethod(st017b_ic) != "smarttype-us") {
                std::cerr << "FAIL ST-017: focus/activate IC IM not smarttype-us\n";
                std::abort();
            }
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_Escape), false);
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_r), false);
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_f), false);
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_r), false);
            if (instance.inputMethod(st017b_ic) != "smarttype-us" ||
                st017b_ic->inputPanel().clientPreedit().toString() != "как") {
                std::cerr << "FAIL ST-017/ST-041: reactivated IC did not translate with "
                             "deferred physical IM; preedit='"
                          << st017b_ic->inputPanel().clientPreedit().toString()
                          << "' im=" << instance.inputMethod(st017b_ic) << "\n";
                std::abort();
            }
            frontend->call<ITestFrontend::pushCommitExpectation>(
                st017b_ic->inputPanel().clientPreedit().toString() + " ");
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_space), false);
            if (instance.inputMethod(st017b_ic) != "smarttype") {
                std::cerr << "FAIL ST-041: reactivated IC did not flush target IM at Space\n";
                std::abort();
            }

            // 4) Two sequential Ctrl+Shift+Space toggles cancel each other
            // without crossing an IM boundary while client preedit is live.
            instance.setCurrentInputMethod(st017b_ic, "smarttype-us", false);
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_t), false);
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_e), false);
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_s), false);
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_t), false);
            const std::string im_before = instance.inputMethod(st017b_ic);
            int manual_switch_count = 0;
            auto manual_switch_watcher = instance.watchEvent(
                EventType::InputContextSwitchInputMethod, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& switch_event =
                        static_cast<InputContextSwitchInputMethodEvent&>(raw_event);
                    if (switch_event.inputContext() == st017b_ic) {
                        ++manual_switch_count;
                    }
                });
            frontend->call<ITestFrontend::sendKeyEvent>(st017b_uuid, Key("Control+Shift+space"),
                                                       false);
            const std::string im_mid = instance.inputMethod(st017b_ic);
            if (im_mid != im_before ||
                st017b_ic->inputPanel().clientPreedit().toString() != "еуые") {
                std::cerr << "FAIL ST-041: manual buffer toggle crossed a live-preedit IM "
                             "boundary\n";
                std::abort();
            }
            // Physical EN is intentionally unchanged; the next queued EN key
            // must be coerced through the deferred RU target.
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_f), false);
            if (st017b_ic->inputPanel().clientPreedit().toString() != "еуыеа") {
                std::cerr << "FAIL ST-041: manual deferred target did not coerce queued key\n";
                std::abort();
            }
            frontend->call<ITestFrontend::keyEvent>(st017b_uuid, Key(FcitxKey_BackSpace), false);
            frontend->call<ITestFrontend::sendKeyEvent>(st017b_uuid, Key("Control+Shift+space"),
                                                       false);
            const std::string im_after = instance.inputMethod(st017b_ic);
            if (im_after != im_before || manual_switch_count != 0 ||
                st017b_ic->inputPanel().clientPreedit().toString() != "test") {
                std::cerr << "FAIL ST-041: double manual toggle did not cancel deferred IM "
                             "state\n";
                std::abort();
            }

            runtime_store.set_setting("sentence_capitalization", true);
            frontend->call<ITestFrontend::destroyInputContext>(st017_uuid);
            frontend->call<ITestFrontend::destroyInputContext>(st017b_uuid);
        }

        // ST-021: phrase layout rewrite — "F ns xnj" → rewrite "F ns " when "xnj"/"что" fixes.
        {
            runtime_store.set_string_setting("layout_mode", "auto");
            runtime_store.set_setting("layout_correction", true);
            runtime_store.set_setting("sentence_capitalization", false);
            runtime_store.set_setting("inline_correction_flash", false);
            instance.setCurrentInputMethod(test_context, "smarttype-us", false);

            CapabilityFlags flags = CapabilityFlag::Preedit;
            flags |= CapabilityFlag::SurroundingText;
            test_context->setCapabilityFlags(flags);

            // Short tokens stay as typed (len safety); they become the phrase prefix.
            frontend->call<ITestFrontend::pushCommitExpectation>("F ");
            send_ascii("F");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);

            frontend->call<ITestFrontend::pushCommitExpectation>("ns ");
            send_ascii("ns");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);

            // Document before cursor matches the two commits (required for phrase rewrite).
            test_context->surroundingText().setText("F ns ", 5, 5);
            test_context->updateSurroundingText();

            // "xnj" → proactive mid-word "что" at len 3, then space commits.
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_x), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_n), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_j), false);

            // Phrase rewrite commits "А ты " then current token "что ".
            frontend->call<ITestFrontend::pushCommitExpectation>("А ты ");
            frontend->call<ITestFrontend::pushCommitExpectation>("что ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);

            if (instance.inputMethod(test_context) != "smarttype") {
                std::cerr << "FAIL ST-021: expected IM smarttype after phrase layout fix\n";
                std::abort();
            }

            // ST-026b: 3-letter EN→RU layout ("xnj") must auto-commit on Space even
            // without Cyrillic context — otherwise only a candidate is shown and the
            // typed Latin is committed (user saw "что" in panel then Space kept "xnj").
            // Also Backspace after Space must undo layout (was missing for proactive keep).
            {
                runtime_store.set_string_setting("layout_mode", "auto");
                runtime_store.set_setting("layout_correction", true);
                runtime_store.set_setting("sentence_capitalization", false);
                instance.setCurrentInputMethod(test_context, "smarttype-us", false);
                frontend->call<ITestFrontend::pushCommitExpectation>("что ");
                send_ascii("xnj");
                frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
                if (instance.inputMethod(test_context) != "smarttype") {
                    std::cerr << "FAIL ST-026b: expected IM smarttype after xnj→что\n";
                    std::abort();
                }
                // Surrounding must reflect the commit so undo can delete+restore immediately.
                test_context->surroundingText().setText("что ", 4, 4);
                test_context->updateSurroundingText();
                frontend->call<ITestFrontend::pushCommitExpectation>("xnj ");
                frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_BackSpace),
                                                       false);
                instance.setCurrentInputMethod(test_context, "smarttype-us", false);
            }

            // ST-026: Chromium-like clients must delete phrase prefix via Backspace,
            // not deleteSurroundingText (which Chrome often ignores → "F ns А ты что").
            {
                runtime_store.set_string_setting("layout_mode", "auto");
                runtime_store.set_setting("layout_correction", true);
                runtime_store.set_setting("sentence_capitalization", false);
                runtime_store.set_setting("inline_correction_flash", false);

                const auto chrome_uuid =
                    frontend->call<ITestFrontend::createInputContext>("google-chrome");
                auto* chrome_ic = instance.inputContextManager().findByUUID(chrome_uuid);
                if (!chrome_ic) {
                    std::cerr << "FAIL ST-026: chrome test IC missing\n";
                    std::abort();
                }
                CapabilityFlags chrome_flags = CapabilityFlag::Preedit;
                chrome_flags |= CapabilityFlag::SurroundingText;
                chrome_ic->setCapabilityFlags(chrome_flags);
                instance.setCurrentInputMethod(chrome_ic, "smarttype-us", false);

                int backspace_forwards = 0;
                auto bs_watcher = instance.watchEvent(
                    EventType::InputContextForwardKey, EventWatcherPhase::Default,
                    [&](Event& raw_event) {
                        auto& key_event = static_cast<ForwardKeyEvent&>(raw_event);
                        if (key_event.inputContext() == chrome_ic &&
                            key_event.key().check(FcitxKey_BackSpace)) {
                            ++backspace_forwards;
                        }
                    });

                auto send_chrome = [&](const std::string& value) {
                    for (char ch : value) {
                        frontend->call<ITestFrontend::keyEvent>(
                            chrome_uuid, Key(std::string(1, ch)), false);
                    }
                };

                // Chromium uses immediate literal commits. This avoids cumulative
                // preedit duplication in controlled React inputs that reset the
                // Wayland text-input transaction after value synchronisation.
                frontend->call<ITestFrontend::pushCommitExpectation>("F");
                frontend->call<ITestFrontend::pushCommitExpectation>(" ");
                send_chrome("F");
                frontend->call<ITestFrontend::keyEvent>(chrome_uuid, Key(FcitxKey_space),
                                                       false);

                frontend->call<ITestFrontend::pushCommitExpectation>("n");
                frontend->call<ITestFrontend::pushCommitExpectation>("s");
                frontend->call<ITestFrontend::pushCommitExpectation>(" ");
                send_chrome("ns");
                frontend->call<ITestFrontend::keyEvent>(chrome_uuid, Key(FcitxKey_space),
                                                       false);

                chrome_ic->surroundingText().setText("F ns ", 5, 5);
                chrome_ic->updateSurroundingText();

                frontend->call<ITestFrontend::pushCommitExpectation>("x");
                frontend->call<ITestFrontend::pushCommitExpectation>("n");
                frontend->call<ITestFrontend::pushCommitExpectation>("что");
                frontend->call<ITestFrontend::keyEvent>(chrome_uuid, Key(FcitxKey_x), false);
                frontend->call<ITestFrontend::keyEvent>(chrome_uuid, Key(FcitxKey_n), false);
                frontend->call<ITestFrontend::keyEvent>(chrome_uuid, Key(FcitxKey_j), false);

                frontend->call<ITestFrontend::pushCommitExpectation>("А ты ");
                // "что" was already committed by the proactive immediate path;
                // the word boundary now commits only its delimiter.
                frontend->call<ITestFrontend::pushCommitExpectation>(" ");
                frontend->call<ITestFrontend::keyEvent>(chrome_uuid, Key(FcitxKey_space),
                                                       false);

                // "F ns " = 5 characters → at least 5 Backspace forwards on Chrome path.
                if (backspace_forwards < 5) {
                    std::cerr << "FAIL ST-026: expected >=5 Backspace forwards for Chrome "
                                 "phrase delete, got "
                              << backspace_forwards << "\n";
                    std::abort();
                }
                if (instance.inputMethod(chrome_ic) != "smarttype") {
                    std::cerr << "FAIL ST-026: expected smarttype after Chrome phrase fix\n";
                    std::abort();
                }

                // Regression for the exact reported city-field failure. Every
                // physical key is committed exactly once; no cumulative preedit
                // ("М", "Ма", "Маха"...) is ever sent to the controlled input.
                const Key city[] = {
                    Key(FcitxKey_Cyrillic_em), Key(FcitxKey_Cyrillic_a),
                    Key(FcitxKey_Cyrillic_ha), Key(FcitxKey_Cyrillic_a),
                    Key(FcitxKey_Cyrillic_che), Key(FcitxKey_Cyrillic_ka),
                    Key(FcitxKey_Cyrillic_a), Key(FcitxKey_Cyrillic_el),
                    Key(FcitxKey_Cyrillic_a)};
                for (const auto& letter : {"м", "а", "х", "а", "ч", "к", "а", "л", "а"}) {
                    frontend->call<ITestFrontend::pushCommitExpectation>(letter);
                }
                frontend->call<ITestFrontend::pushCommitExpectation>(" ");
                for (const auto& key : city) {
                    frontend->call<ITestFrontend::keyEvent>(chrome_uuid, key, false);
                }
                frontend->call<ITestFrontend::keyEvent>(chrome_uuid, Key(FcitxKey_space),
                                                       false);

                frontend->call<ITestFrontend::destroyInputContext>(chrome_uuid);
                instance.setCurrentInputMethod(test_context, "smarttype", false);
            }

            // ST-021 reverse: "ш ерштл" → "i think" (character-based SurroundingText).
            // Cursor is in *characters*: "ш " = 2 chars (not 3 bytes).
            instance.setCurrentInputMethod(test_context, "smarttype", false);
            frontend->call<ITestFrontend::pushCommitExpectation>("ш ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_sha), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);

            int reverse_switch_count = 0;
            std::string reverse_last_preedit_update = "<none>";
            std::vector<std::string> reverse_preedit_at_switch;
            auto reverse_preedit_watcher = instance.watchEvent(
                EventType::InputContextUpdatePreedit, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& preedit_event = static_cast<InputContextEvent&>(raw_event);
                    if (preedit_event.inputContext() == test_context) {
                        reverse_last_preedit_update =
                            test_context->inputPanel().clientPreedit().toString();
                    }
                });
            auto reverse_switch_watcher = instance.watchEvent(
                EventType::InputContextSwitchInputMethod, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& switch_event =
                        static_cast<InputContextSwitchInputMethodEvent&>(raw_event);
                    if (switch_event.inputContext() == test_context) {
                        ++reverse_switch_count;
                        reverse_preedit_at_switch.push_back(reverse_last_preedit_update);
                    }
                });

            // Type ерштл with proactive mid-word; remaining key after switch may still
            // arrive as Cyrillic and must be coerced to Latin ("think", not "thinл").
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_ie), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_er), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_sha), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_te), false);
            // After proactive RU→EN, buffer should be Latin; next key still Cyrillic "л".
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_el), false);

            const std::string pre_space =
                test_context->inputPanel().clientPreedit().toString();
            if (pre_space != "think" && pre_space != "Think") {
                std::cerr << "FAIL ST-021 reverse: expected preedit think after coerce, got '"
                          << pre_space << "'\n";
                std::abort();
            }
            if (instance.inputMethod(test_context) != "smarttype") {
                std::cerr << "FAIL ST-041 reverse: physical IM changed before preedit commit\n";
                std::abort();
            }

            test_context->surroundingText().setText("ш ", 2, 2);
            test_context->updateSurroundingText();

            frontend->call<ITestFrontend::pushCommitExpectation>("i ");
            frontend->call<ITestFrontend::pushCommitExpectation>("think ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
            if (instance.inputMethod(test_context) != "smarttype-us") {
                std::cerr << "FAIL ST-041 reverse: deferred EN target did not flush at Space\n";
                std::abort();
            }
            if (reverse_switch_count != 1 || reverse_preedit_at_switch.size() != 1 ||
                !reverse_preedit_at_switch.front().empty()) {
                std::cerr << "FAIL ST-041 reverse: switch lacked an explicit empty-preedit "
                             "update\n";
                std::abort();
            }

            // "вщ нщг" → "do you" (2-letter RU→EN auto on first word + layout of second).
            instance.setCurrentInputMethod(test_context, "smarttype", false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_ve), false);
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_Cyrillic_shcha), false);
            frontend->call<ITestFrontend::pushCommitExpectation>("do ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);

            // After switch to EN, type "you" as Latin (real keys after layout flip).
            send_ascii("you");
            frontend->call<ITestFrontend::pushCommitExpectation>("you ");
            frontend->call<ITestFrontend::keyEvent>(test_uuid, Key(FcitxKey_space), false);
        }

        // ST-041: a client-side inline correction remains preedit until its
        // delayed commit. Both Backspace cancellation and the eventual commit
        // must avoid a physical switch while that preedit is live.
        {
            runtime_store.set_string_setting("layout_mode", "auto");
            runtime_store.set_setting("layout_correction", true);
            runtime_store.set_setting("inline_correction_flash", true);
            runtime_store.set_setting("sentence_capitalization", false);

            const auto delayed_uuid =
                frontend->call<ITestFrontend::createInputContext>("st041-delayed-client");
            auto* delayed_ic = instance.inputContextManager().findByUUID(delayed_uuid);
            CapabilityFlags delayed_flags = CapabilityFlag::Preedit;
            delayed_flags |= CapabilityFlag::ClientSideInputPanel;
            delayed_ic->setCapabilityFlags(delayed_flags);
            instance.setCurrentInputMethod(delayed_ic, "smarttype", false);

            int delayed_switch_count = 0;
            std::string delayed_last_preedit_update = "<none>";
            std::vector<std::string> delayed_preedit_at_switch;
            auto delayed_preedit_watcher = instance.watchEvent(
                EventType::InputContextUpdatePreedit, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& preedit_event = static_cast<InputContextEvent&>(raw_event);
                    if (preedit_event.inputContext() == delayed_ic) {
                        delayed_last_preedit_update =
                            delayed_ic->inputPanel().clientPreedit().toString();
                    }
                });
            auto delayed_switch_watcher = instance.watchEvent(
                EventType::InputContextSwitchInputMethod, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& switch_event =
                        static_cast<InputContextSwitchInputMethodEvent&>(raw_event);
                    if (switch_event.inputContext() == delayed_ic) {
                        ++delayed_switch_count;
                        delayed_preedit_at_switch.push_back(delayed_last_preedit_update);
                    }
                });

            const auto type_do_on_ru = [&]() {
                frontend->call<ITestFrontend::keyEvent>(delayed_uuid,
                                                       Key(FcitxKey_Cyrillic_ve), false);
                frontend->call<ITestFrontend::keyEvent>(delayed_uuid,
                                                       Key(FcitxKey_Cyrillic_shcha), false);
                frontend->call<ITestFrontend::keyEvent>(delayed_uuid, Key(FcitxKey_space),
                                                       false);
            };

            type_do_on_ru();
            if (delayed_ic->inputPanel().clientPreedit().toString() != "do " ||
                instance.inputMethod(delayed_ic) != "smarttype") {
                std::cerr << "FAIL ST-041: delayed layout correction switched live preedit\n";
                std::abort();
            }
            frontend->call<ITestFrontend::keyEvent>(delayed_uuid, Key(FcitxKey_BackSpace), false);
            if (delayed_ic->inputPanel().clientPreedit().toString() != "вщ" ||
                delayed_switch_count != 0 || instance.inputMethod(delayed_ic) != "smarttype") {
                std::cerr << "FAIL ST-041: delayed Backspace caused a transient IM switch\n";
                std::abort();
            }
            frontend->call<ITestFrontend::keyEvent>(delayed_uuid, Key(FcitxKey_BackSpace), false);
            frontend->call<ITestFrontend::keyEvent>(delayed_uuid, Key(FcitxKey_BackSpace), false);

            type_do_on_ru();
            frontend->call<ITestFrontend::pushCommitExpectation>("do ");
            frontend->call<ITestFrontend::keyEvent>(delayed_uuid, Key(FcitxKey_Return), false);
            if (delayed_switch_count != 1 || delayed_preedit_at_switch.size() != 1 ||
                !delayed_preedit_at_switch.front().empty() ||
                instance.inputMethod(delayed_ic) != "smarttype-us") {
                std::cerr << "FAIL ST-041: delayed commit did not flush after empty preedit\n";
                std::abort();
            }

            frontend->call<ITestFrontend::destroyInputContext>(delayed_uuid);
            runtime_store.set_setting("inline_correction_flash", false);
            runtime_store.set_setting("sentence_capitalization", true);
        }

        // GNOME's IBus bridge and X11 desktops may leave the physical XKB
        // group on US even when Fcitx has selected SmartType Russian. Coerce
        // those keysyms to the logical SmartType layout instead of leaking
        // Latin text or US punctuation.
        {
            runtime_store.set_setting("x11_normalize_layout", true);
            const auto x11_uuid =
                frontend->call<ITestFrontend::createInputContext>("x11-xim-layout-lag");
            auto* x11_context = instance.inputContextManager().findByUUID(x11_uuid);
            x11_context->setCapabilityFlags(CapabilityFlag::Preedit);
            instance.setCurrentInputMethod(x11_context, "smarttype", false);
            for (const char key : std::string("ghbdtn")) {
                frontend->call<ITestFrontend::keyEvent>(
                    x11_uuid, Key(std::string(1, key)), false);
            }
            if (x11_context->inputPanel().clientPreedit().toString() != "Привет") {
                std::cerr << "FAIL X11: US keysyms were not normalized to SmartType RU\n";
                std::abort();
            }
            frontend->call<ITestFrontend::destroyInputContext>(x11_uuid);

            runtime_store.set_setting("sentence_capitalization", false);
            runtime_store.set_setting("autocorrect", false);
            runtime_store.set_setting("auto_space_after_punctuation", false);
            const auto punctuation_uuid =
                frontend->call<ITestFrontend::createInputContext>("logical-ru-punctuation-row");
            auto* punctuation_context =
                instance.inputContextManager().findByUUID(punctuation_uuid);
            punctuation_context->setCapabilityFlags(CapabilityFlag::Preedit);
            instance.setCurrentInputMethod(punctuation_context, "smarttype", false);
            const Key punctuation_keys[] = {
                Key(FcitxKey_bracketleft, KeyStates{}, 34),
                Key(FcitxKey_bracketright, KeyStates{}, 35),
                Key(FcitxKey_semicolon, KeyStates{}, 47),
                Key(FcitxKey_apostrophe, KeyStates{}, 48),
                Key(FcitxKey_comma, KeyStates{}, 59),
                Key(FcitxKey_period, KeyStates{}, 60)};
            for (const auto& key : punctuation_keys) {
                frontend->call<ITestFrontend::keyEvent>(punctuation_uuid, key, false);
            }
            if (punctuation_context->inputPanel().clientPreedit().toString() !=
                "хъжэбю") {
                std::cerr << "FAIL layout normalization: [ ] ; ' , . did not become "
                             "х ъ ж э б ю\n";
                std::abort();
            }
            frontend->call<ITestFrontend::pushCommitExpectation>("хъжэбю.");
            frontend->call<ITestFrontend::keyEvent>(
                punctuation_uuid, Key(FcitxKey_slash, KeyStates{}, 61), false);
            frontend->call<ITestFrontend::destroyInputContext>(punctuation_uuid);

            const auto shifted_uuid =
                frontend->call<ITestFrontend::createInputContext>("logical-ru-shift-row");
            auto* shifted_context = instance.inputContextManager().findByUUID(shifted_uuid);
            shifted_context->setCapabilityFlags(CapabilityFlag::Preedit);
            instance.setCurrentInputMethod(shifted_context, "smarttype", false);
            const Key shifted_keys[] = {
                Key(FcitxKey_braceleft, KeyState::Shift, 34),
                Key(FcitxKey_braceright, KeyState::Shift, 35),
                Key(FcitxKey_colon, KeyState::Shift, 47),
                Key(FcitxKey_quotedbl, KeyState::Shift, 48),
                Key(FcitxKey_less, KeyState::Shift, 59),
                Key(FcitxKey_greater, KeyState::Shift, 60)};
            for (const auto& key : shifted_keys) {
                frontend->call<ITestFrontend::keyEvent>(shifted_uuid, key, false);
            }
            if (shifted_context->inputPanel().clientPreedit().toString() !=
                "ХЪЖЭБЮ") {
                std::cerr << "FAIL layout normalization: shifted punctuation row\n";
                std::abort();
            }
            frontend->call<ITestFrontend::pushCommitExpectation>("ХЪЖЭБЮ,");
            frontend->call<ITestFrontend::keyEvent>(
                shifted_uuid, Key(FcitxKey_question, KeyState::Shift, 61), false);
            frontend->call<ITestFrontend::destroyInputContext>(shifted_uuid);

            runtime_store.set_setting("x11_normalize_layout", false);
            runtime_store.set_setting("sentence_capitalization", true);
            runtime_store.set_setting("autocorrect", true);
            runtime_store.set_setting("auto_space_after_punctuation", true);
        }

        // GNOME Wayland exposes ordinary application text fields through its
        // IBus compositor bridge with program=gnome-shell. Treating that
        // program name as shell chrome disables SmartType in every GNOME app.
        // Its live capability mask is 0x150052: client preedit and surrounding
        // text are present, but ClientSideInputPanel and CommitStringWithCursor
        // are not. This path must finish every correction transaction before
        // accepting the next word.
        {
            runtime_store.set_string_setting("layout_mode", "auto");
            runtime_store.set_setting("autocorrect", true);
            runtime_store.set_setting("sentence_capitalization", false);
            runtime_store.set_setting("inline_correction_flash", true);
            const auto gnome_uuid =
                frontend->call<ITestFrontend::createInputContext>("gnome-shell");
            auto* gnome_context = instance.inputContextManager().findByUUID(gnome_uuid);
            CapabilityFlags gnome_flags = CapabilityFlag::Preedit;
            gnome_flags |= CapabilityFlag::FormattedPreedit;
            gnome_flags |= CapabilityFlag::SurroundingText;
            gnome_flags |= CapabilityFlag::SpellCheck;
            gnome_flags |= CapabilityFlag::WordCompletion;
            gnome_flags |= CapabilityFlag::UppwercaseSentences;
            gnome_context->setCapabilityFlags(gnome_flags);
            instance.setCurrentInputMethod(gnome_context, "smarttype", false);

            const Key gnome_typo[] = {
                Key(FcitxKey_Cyrillic_es), Key(FcitxKey_Cyrillic_ie),
                Key(FcitxKey_Cyrillic_ve), Key(FcitxKey_Cyrillic_o),
                Key(FcitxKey_Cyrillic_de), Key(FcitxKey_Cyrillic_en),
                Key(FcitxKey_Cyrillic_ya)};

            // Tab commits a candidate only after explicitly clearing the IBus
            // preedit. CommitText-before-empty-preedit leaves GNOME's caret at
            // the beginning of the old composition.
            bool candidate_commit_saw_empty_preedit = false;
            auto gnome_commit_watcher = instance.watchEvent(
                EventType::InputContextCommitString, EventWatcherPhase::Default,
                [&](Event& raw_event) {
                    auto& commit_event = static_cast<CommitStringEvent&>(raw_event);
                    if (commit_event.inputContext() == gnome_context &&
                        commit_event.text() == "сегодня ") {
                        candidate_commit_saw_empty_preedit =
                            gnome_context->inputPanel().clientPreedit().empty();
                    }
                });
            for (const auto& key : gnome_typo) {
                frontend->call<ITestFrontend::keyEvent>(gnome_uuid, key, false);
            }
            if (gnome_context->inputPanel().candidateList() == nullptr) {
                std::cerr << "FAIL GNOME: expected candidates before Tab\n";
                std::abort();
            }
            frontend->call<ITestFrontend::pushCommitExpectation>("сегодня ");
            frontend->call<ITestFrontend::keyEvent>(gnome_uuid, Key(FcitxKey_Tab), false);
            if (!candidate_commit_saw_empty_preedit ||
                !gnome_context->inputPanel().clientPreedit().empty()) {
                std::cerr << "FAIL GNOME: candidate committed before clearing IBus preedit\n";
                std::abort();
            }
            gnome_commit_watcher.reset();

            // Exact owner regression: physical keys for
            // "привет рщц фку нщг друг " must produce one complete commit per
            // word. In particular, "how " must not remain as a 160 ms preedit
            // and absorb the first letters of "are".
            const Key privet_keys[] = {
                Key(FcitxKey_Cyrillic_pe), Key(FcitxKey_Cyrillic_er),
                Key(FcitxKey_Cyrillic_i), Key(FcitxKey_Cyrillic_ve),
                Key(FcitxKey_Cyrillic_ie), Key(FcitxKey_Cyrillic_te)};
            frontend->call<ITestFrontend::pushCommitExpectation>("привет ");
            for (const auto& key : privet_keys) {
                frontend->call<ITestFrontend::keyEvent>(gnome_uuid, key, false);
            }
            frontend->call<ITestFrontend::keyEvent>(gnome_uuid, Key(FcitxKey_space), false);

            const Key how_on_ru[] = {
                Key(FcitxKey_Cyrillic_er), Key(FcitxKey_Cyrillic_shcha),
                Key(FcitxKey_Cyrillic_tse)};
            frontend->call<ITestFrontend::pushCommitExpectation>("how ");
            for (const auto& key : how_on_ru) {
                frontend->call<ITestFrontend::keyEvent>(gnome_uuid, key, false);
            }
            frontend->call<ITestFrontend::keyEvent>(gnome_uuid, Key(FcitxKey_space), false);
            if (!gnome_context->inputPanel().clientPreedit().empty()) {
                std::cerr << "FAIL GNOME: layout correction leaked into delayed preedit\n";
                std::abort();
            }

            const auto send_gnome_ascii = [&](std::string_view text) {
                for (const char value : text) {
                    frontend->call<ITestFrontend::keyEvent>(
                        gnome_uuid, Key(std::string(1, value)), false);
                }
            };
            frontend->call<ITestFrontend::pushCommitExpectation>("are ");
            send_gnome_ascii("are");
            frontend->call<ITestFrontend::keyEvent>(gnome_uuid, Key(FcitxKey_space), false);
            frontend->call<ITestFrontend::pushCommitExpectation>("you ");
            send_gnome_ascii("you");
            frontend->call<ITestFrontend::keyEvent>(gnome_uuid, Key(FcitxKey_space), false);
            frontend->call<ITestFrontend::pushCommitExpectation>("друг ");
            send_gnome_ascii("lheu");
            frontend->call<ITestFrontend::keyEvent>(gnome_uuid, Key(FcitxKey_space), false);
            if (!gnome_context->inputPanel().clientPreedit().empty()) {
                std::cerr << "FAIL GNOME: exact phrase left an active preedit\n";
                std::abort();
            }
            frontend->call<ITestFrontend::destroyInputContext>(gnome_uuid);
            runtime_store.set_setting("inline_correction_flash", false);
        }

        // ST-034: clients without SurroundingText must still autocorrect, undo
        // via forwarded Backspaces, and continue typing without duplication.
        {
            runtime_store.set_string_setting("layout_mode", "suggest");
            runtime_store.set_setting("autocorrect", true);
            runtime_store.set_setting("sentence_capitalization", false);
            const auto no_surrounding_uuid =
                frontend->call<ITestFrontend::createInputContext>("no-surrounding-app");
            auto* no_surrounding_context =
                instance.inputContextManager().findByUUID(no_surrounding_uuid);
            no_surrounding_context->setCapabilityFlags(CapabilityFlag::Preedit);
            instance.setCurrentInputMethod(no_surrounding_context, "smarttype", false);

            const Key typo_no_surrounding[] = {
                Key(FcitxKey_Cyrillic_es), Key(FcitxKey_Cyrillic_ie),
                Key(FcitxKey_Cyrillic_ve), Key(FcitxKey_Cyrillic_o),
                Key(FcitxKey_Cyrillic_de), Key(FcitxKey_Cyrillic_en),
                Key(FcitxKey_Cyrillic_ya)};
            frontend->call<ITestFrontend::pushCommitExpectation>("сегодня ");
            for (const auto& key : typo_no_surrounding) {
                frontend->call<ITestFrontend::keyEvent>(no_surrounding_uuid, key, false);
            }
            frontend->call<ITestFrontend::keyEvent>(no_surrounding_uuid,
                                                    Key(FcitxKey_space), false);

            // Fallback undo forwards one Backspace per committed character and
            // commits the original after a 50 ms timer. Keep this IC alive and
            // schedule the next assertion after that timer has fired.
            frontend->call<ITestFrontend::pushCommitExpectation>("севодня ");
            frontend->call<ITestFrontend::keyEvent>(no_surrounding_uuid,
                                                    Key(FcitxKey_BackSpace), false);

            delayed_exit_timer = instance.eventLoop().addTimeEvent(
                CLOCK_MONOTONIC, fcitx::now(CLOCK_MONOTONIC) + 100000, 0,
                [&instance, frontend, no_surrounding_uuid, test_uuid](
                    fcitx::EventSourceTime*, uint64_t) {
                    // A second commit makes an unconsumed undo expectation fail,
                    // and proves that ordinary typing still works afterwards.
                    const auto continued_uuid =
                        frontend->call<ITestFrontend::createInputContext>(
                            "no-surrounding-continued-app");
                    auto* continued_context =
                        instance.inputContextManager().findByUUID(continued_uuid);
                    continued_context->setCapabilityFlags(CapabilityFlag::Preedit);
                    instance.setCurrentInputMethod(continued_context, "smarttype", false);
                    const Key plain_no_surrounding[] = {
                        Key(FcitxKey_Cyrillic_pe), Key(FcitxKey_Cyrillic_er),
                        Key(FcitxKey_Cyrillic_i), Key(FcitxKey_Cyrillic_ve),
                        Key(FcitxKey_Cyrillic_ie), Key(FcitxKey_Cyrillic_te)};
                    frontend->call<ITestFrontend::pushCommitExpectation>("привет ");
                    for (const auto& key : plain_no_surrounding) {
                        frontend->call<ITestFrontend::keyEvent>(continued_uuid, key,
                                                                false);
                    }
                    frontend->call<ITestFrontend::keyEvent>(continued_uuid,
                                                            Key(FcitxKey_space), false);
                    frontend->call<ITestFrontend::destroyInputContext>(continued_uuid);
                    frontend->call<ITestFrontend::destroyInputContext>(
                        no_surrounding_uuid);
                    frontend->call<ITestFrontend::destroyInputContext>(test_uuid);
                    instance.exit();
                    return false;
                });
        }
    });
    instance.exec();
    if (global_server_thread.joinable()) {
        global_server_thread.join();
    }
    return 0;
}
