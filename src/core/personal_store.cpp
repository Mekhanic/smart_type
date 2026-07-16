#include "smarttype/personal_store.hpp"
#include "smarttype/text.hpp"

#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <sqlite3.h>

namespace smarttype {
namespace {

void check(int code, sqlite3* db, const char* operation) {
    if (code != SQLITE_OK && code != SQLITE_DONE && code != SQLITE_ROW) {
        throw std::runtime_error(std::string(operation) + ": " + sqlite3_errmsg(db));
    }
}

class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        check(sqlite3_prepare_v2(db, sql, -1, &statement_, nullptr), db, "prepare database statement");
    }
    ~Statement() { sqlite3_finalize(statement_); }
    sqlite3_stmt* get() const { return statement_; }
private:
    sqlite3* db_;
    sqlite3_stmt* statement_{nullptr};
};

void bind(sqlite3* db, sqlite3_stmt* statement, int index, const std::string& value) {
    check(sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT), db, "bind text");
}

std::size_t utf8_length(std::string_view value) {
    return static_cast<std::size_t>(std::count_if(value.begin(), value.end(), [](unsigned char byte) {
        return (byte & 0xC0U) != 0x80U;
    }));
}

}  // namespace

std::filesystem::path PersonalStore::default_path() {
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        return std::filesystem::path(xdg) / "smarttype" / "personal.sqlite3";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".local/share/smarttype/personal.sqlite3";
    }
    throw std::runtime_error("HOME and XDG_DATA_HOME are not set");
}

PersonalStore::PersonalStore(std::filesystem::path path) : path_(std::move(path)) {
    if (path_ != ":memory:") {
        std::filesystem::create_directories(path_.parent_path());
        if (path_.parent_path().filename() == "smarttype") {
            std::filesystem::permissions(
                path_.parent_path(), std::filesystem::perms::owner_all,
                std::filesystem::perm_options::replace);
        }
    }
    check(sqlite3_open_v2(path_.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr), db_, "open database");
    if (path_ != ":memory:") {
        std::filesystem::permissions(
            path_, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace);
    }
    sqlite3_busy_timeout(db_, 1000);
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
    exec("CREATE TABLE IF NOT EXISTS personal_words("
         "word TEXT PRIMARY KEY, use_count INTEGER NOT NULL DEFAULT 1, updated_at INTEGER NOT NULL DEFAULT(unixepoch()));");
    exec("CREATE TABLE IF NOT EXISTS learned_words("
         "word TEXT PRIMARY KEY, use_count INTEGER NOT NULL DEFAULT 1, updated_at INTEGER NOT NULL DEFAULT(unixepoch()));");
    exec("CREATE TABLE IF NOT EXISTS correction_rules("
         "typo TEXT NOT NULL, correction TEXT NOT NULL, previous_word TEXT NOT NULL DEFAULT '', "
         "confidence REAL NOT NULL DEFAULT 0.99, accepts INTEGER NOT NULL DEFAULT 1, undos INTEGER NOT NULL DEFAULT 0, "
         "updated_at INTEGER NOT NULL DEFAULT(unixepoch()), PRIMARY KEY(typo, correction, previous_word));");
    exec("CREATE TABLE IF NOT EXISTS learned_rules("
         "typo TEXT NOT NULL, correction TEXT NOT NULL, previous_word TEXT NOT NULL DEFAULT '', "
         "confidence REAL NOT NULL DEFAULT 0.99, accepts INTEGER NOT NULL DEFAULT 1, "
         "updated_at INTEGER NOT NULL DEFAULT(unixepoch()), PRIMARY KEY(typo,correction,previous_word));");
    exec("CREATE TABLE IF NOT EXISTS word_transitions("
         "word TEXT NOT NULL, next_word TEXT NOT NULL, use_count INTEGER NOT NULL DEFAULT 1, "
         "updated_at INTEGER NOT NULL DEFAULT(unixepoch()), PRIMARY KEY(word, next_word));");
    exec("CREATE TABLE IF NOT EXISTS app_blacklist("
         "app_name TEXT PRIMARY KEY);");
    exec("CREATE TABLE IF NOT EXISTS unknown_word_observations("
         "word TEXT PRIMARY KEY, seen_count INTEGER NOT NULL DEFAULT 1, "
         "updated_at INTEGER NOT NULL DEFAULT(unixepoch()));");
    exec("CREATE TABLE IF NOT EXISTS settings("
         "key TEXT PRIMARY KEY, value TEXT NOT NULL);");
    // A newly installed SmartType must work before the tray or settings UI is
    // opened. Keep an explicit user choice of 0 on upgrades.
    exec("INSERT OR IGNORE INTO settings(key,value) VALUES('enabled','1');");
    exec("CREATE TABLE IF NOT EXISTS correction_history("
         "id INTEGER PRIMARY KEY AUTOINCREMENT, original TEXT NOT NULL, replacement TEXT NOT NULL, "
         "app TEXT NOT NULL DEFAULT '', source TEXT NOT NULL, undone INTEGER NOT NULL DEFAULT 0, "
         "created_at INTEGER NOT NULL DEFAULT(unixepoch()));");
    exec("CREATE TABLE IF NOT EXISTS correction_feedback("
         "typo TEXT NOT NULL, correction TEXT NOT NULL, previous_word TEXT NOT NULL DEFAULT '', "
         "accepts INTEGER NOT NULL DEFAULT 0, undos INTEGER NOT NULL DEFAULT 0, "
         "updated_at INTEGER NOT NULL DEFAULT(unixepoch()), "
         "PRIMARY KEY(typo,correction,previous_word));");
    exec("CREATE TABLE IF NOT EXISTS diagnostics("
         "id INTEGER PRIMARY KEY AUTOINCREMENT, original TEXT NOT NULL, candidate TEXT NOT NULL, "
         "action TEXT NOT NULL, reason TEXT NOT NULL, confidence REAL NOT NULL, app TEXT NOT NULL DEFAULT '', "
         "created_at INTEGER NOT NULL DEFAULT(unixepoch()));");
    exec("DELETE FROM app_blacklist WHERE app_name IN "
         "('code','sublime','notepad','vim','emacs','nano','clion','idea','studio') "
         "AND NOT EXISTS(SELECT 1 FROM settings WHERE key='blacklist_v2_migrated');");
    exec("INSERT OR IGNORE INTO settings(key,value) VALUES('blacklist_v2_migrated','1');");
    // Older manual-correction tracking could accidentally learn rules such as
    // "и -> включите" after an unrelated Backspace. A one-character token is
    // never safe to rewrite automatically, so remove such poisoned rules once
    // and keep the invariant for future databases.
    exec("DELETE FROM correction_rules WHERE length(typo) <= 1;");
    exec("DELETE FROM learned_rules WHERE length(typo) <= 1;");
}

PersonalStore::~PersonalStore() { if (db_) sqlite3_close(db_); }

void PersonalStore::exec(const char* sql) const {
    char* error = nullptr;
    const int code = sqlite3_exec(db_, sql, nullptr, nullptr, &error);
    if (code != SQLITE_OK) {
        std::string message = error ? error : sqlite3_errmsg(db_);
        sqlite3_free(error);
        throw std::runtime_error("database: " + message);
    }
}

void PersonalStore::add_word(const std::string& word) {
    Statement query(db_, "INSERT INTO personal_words(word) VALUES(?) ON CONFLICT(word) DO UPDATE SET "
                         "use_count=use_count+1, updated_at=unixepoch();");
    bind(db_, query.get(), 1, lowercase_ru(word));
    check(sqlite3_step(query.get()), db_, "add personal word");
}

void PersonalStore::remove_word(const std::string& word) {
    Statement query(db_, "DELETE FROM personal_words WHERE word=?;");
    bind(db_, query.get(), 1, lowercase_ru(word));
    check(sqlite3_step(query.get()), db_, "remove personal word");
}

void PersonalStore::add_learned_word(const std::string& word) {
    Statement query(db_, "INSERT INTO learned_words(word) VALUES(?) ON CONFLICT(word) DO UPDATE SET "
                         "use_count=use_count+1, updated_at=unixepoch();");
    bind(db_, query.get(), 1, lowercase_ru(word));
    check(sqlite3_step(query.get()), db_, "add learned word");
}

bool PersonalStore::observe_unknown_word(const std::string& word, int threshold) {
    if (word.empty() || threshold < 1) return false;
    const std::string normalized = lowercase_ru(word);
    Statement query(db_, "INSERT INTO unknown_word_observations(word) VALUES(?) "
                         "ON CONFLICT(word) DO UPDATE SET seen_count=seen_count+1, updated_at=unixepoch() "
                         "RETURNING seen_count;");
    bind(db_, query.get(), 1, normalized);
    check(sqlite3_step(query.get()), db_, "observe unknown word");
    const int count = sqlite3_column_int(query.get(), 0);
    if (count != threshold) return false;
    add_learned_word(normalized);
    return true;
}

int PersonalStore::unknown_word_observation_count(const std::string& word) const {
    Statement query(db_, "SELECT seen_count FROM unknown_word_observations WHERE word=?;");
    bind(db_, query.get(), 1, lowercase_ru(word));
    return sqlite3_step(query.get()) == SQLITE_ROW ? sqlite3_column_int(query.get(), 0) : 0;
}

void PersonalStore::add_rule(const std::string& typo, const std::string& correction,
                             double confidence, const std::string& previous_word) {
    if (utf8_length(typo) <= 1 || correction.empty()) return;
    Statement query(db_, "INSERT INTO correction_rules(typo,correction,previous_word,confidence) VALUES(?,?,?,?) "
                         "ON CONFLICT(typo,correction,previous_word) DO UPDATE SET accepts=accepts+1, "
                         "confidence=max(confidence,excluded.confidence), updated_at=unixepoch();");
    bind(db_, query.get(), 1, lowercase_ru(typo));
    bind(db_, query.get(), 2, lowercase_ru(correction));
    bind(db_, query.get(), 3, lowercase_ru(previous_word));
    check(sqlite3_bind_double(query.get(), 4, confidence), db_, "bind confidence");
    check(sqlite3_step(query.get()), db_, "add correction rule");
}

void PersonalStore::add_learned_rule(const std::string& typo, const std::string& correction,
                                     double confidence, const std::string& previous_word) {
    if (utf8_length(typo) <= 1 || correction.empty()) return;
    Statement query(db_, "INSERT INTO learned_rules(typo,correction,previous_word,confidence) VALUES(?,?,?,?) "
                         "ON CONFLICT(typo,correction,previous_word) DO UPDATE SET accepts=accepts+1, "
                         "confidence=max(confidence,excluded.confidence), updated_at=unixepoch();");
    bind(db_, query.get(), 1, lowercase_ru(typo));
    bind(db_, query.get(), 2, lowercase_ru(correction));
    bind(db_, query.get(), 3, lowercase_ru(previous_word));
    check(sqlite3_bind_double(query.get(), 4, confidence), db_, "bind learned confidence");
    check(sqlite3_step(query.get()), db_, "add learned rule");
}

void PersonalStore::record_undo(const std::string& typo, const std::string& correction,
                                const std::string& previous_word) {
    Statement query(db_, "INSERT INTO correction_feedback(typo,correction,previous_word,undos) "
                         "VALUES(?,?,?,1) ON CONFLICT(typo,correction,previous_word) DO UPDATE SET "
                         "undos=undos+1, updated_at=unixepoch();");
    bind(db_, query.get(), 1, lowercase_ru(typo));
    bind(db_, query.get(), 2, lowercase_ru(correction));
    bind(db_, query.get(), 3, lowercase_ru(previous_word));
    check(sqlite3_step(query.get()), db_, "record correction undo");
}

void PersonalStore::record_accept(const std::string& typo, const std::string& correction,
                                  const std::string& previous_word) {
    Statement query(db_, "INSERT INTO correction_feedback(typo,correction,previous_word,accepts) "
                         "VALUES(?,?,?,1) ON CONFLICT(typo,correction,previous_word) DO UPDATE SET "
                         "accepts=accepts+1, updated_at=unixepoch();");
    bind(db_, query.get(), 1, lowercase_ru(typo));
    bind(db_, query.get(), 2, lowercase_ru(correction));
    bind(db_, query.get(), 3, lowercase_ru(previous_word));
    check(sqlite3_step(query.get()), db_, "record correction accept");
}

bool PersonalStore::should_demote_correction(const std::string& typo,
                                             const std::string& correction,
                                             const std::string& previous_word) const {
    Statement query(db_, "SELECT accepts,undos FROM correction_feedback "
                         "WHERE typo=? AND correction=? AND previous_word=?;");
    bind(db_, query.get(), 1, lowercase_ru(typo));
    bind(db_, query.get(), 2, lowercase_ru(correction));
    bind(db_, query.get(), 3, lowercase_ru(previous_word));
    if (sqlite3_step(query.get()) == SQLITE_ROW) {
        const int accepts = sqlite3_column_int(query.get(), 0);
        const int undos = sqlite3_column_int(query.get(), 1);
        if (undos >= 2 && undos > accepts) return true;
    }

    // Context remains useful, but three rejections of the same pair across
    // different sentences must still teach the engine to stop auto-applying it.
    Statement aggregate(db_, "SELECT coalesce(sum(accepts),0),coalesce(sum(undos),0) "
                             "FROM correction_feedback WHERE typo=? AND correction=?;");
    bind(db_, aggregate.get(), 1, lowercase_ru(typo));
    bind(db_, aggregate.get(), 2, lowercase_ru(correction));
    if (sqlite3_step(aggregate.get()) != SQLITE_ROW) return false;
    const int accepts = sqlite3_column_int(aggregate.get(), 0);
    const int undos = sqlite3_column_int(aggregate.get(), 1);
    return undos >= 3 && undos > accepts;
}

void PersonalStore::add_transition(const std::string& word, const std::string& next_word) {
    if (word.empty() || next_word.empty()) return;
    Statement query(db_, "INSERT INTO word_transitions(word,next_word) VALUES(?,?) "
                         "ON CONFLICT(word,next_word) DO UPDATE SET "
                         "use_count=use_count+1, updated_at=unixepoch();");
    bind(db_, query.get(), 1, lowercase_ru(word));
    bind(db_, query.get(), 2, lowercase_ru(next_word));
    check(sqlite3_step(query.get()), db_, "add word transition");
}

std::vector<std::string> PersonalStore::next_words(const std::string& word, int limit) const {
    if (word.empty()) return {};
    Statement query(db_, "SELECT next_word FROM word_transitions WHERE word=? "
                         "ORDER BY use_count DESC, updated_at DESC LIMIT ?;");
    bind(db_, query.get(), 1, lowercase_ru(word));
    check(sqlite3_bind_int(query.get(), 2, limit), db_, "bind limit");
    std::vector<std::string> result;
    while (sqlite3_step(query.get()) == SQLITE_ROW) {
        result.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0)));
    }
    return result;
}

int PersonalStore::get_word_use_count(const std::string& word) const {
    if (word.empty()) return 0;
    Statement query(db_, "SELECT coalesce(sum(use_count),0) FROM ("
                         "SELECT use_count FROM personal_words WHERE word=? "
                         "UNION ALL SELECT use_count FROM learned_words WHERE word=?);");
    bind(db_, query.get(), 1, lowercase_ru(word));
    bind(db_, query.get(), 2, lowercase_ru(word));
    return sqlite3_step(query.get()) == SQLITE_ROW ? sqlite3_column_int(query.get(), 0) : 0;
}

int PersonalStore::get_transition_use_count(const std::string& word, const std::string& next_word) const {
    if (word.empty() || next_word.empty()) return 0;
    Statement query(db_, "SELECT use_count FROM word_transitions WHERE word=? AND next_word=?;");
    bind(db_, query.get(), 1, lowercase_ru(word));
    bind(db_, query.get(), 2, lowercase_ru(next_word));
    return sqlite3_step(query.get()) == SQLITE_ROW ? sqlite3_column_int(query.get(), 0) : 0;
}

std::vector<std::string> PersonalStore::words() const {
    Statement query(db_, "SELECT word FROM personal_words ORDER BY word;");
    std::vector<std::string> result;
    while (sqlite3_step(query.get()) == SQLITE_ROW) {
        result.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0)));
    }
    return result;
}

std::vector<std::string> PersonalStore::all_words() const {
    Statement query(db_, "SELECT word FROM personal_words UNION SELECT word FROM learned_words ORDER BY word;");
    std::vector<std::string> result;
    while (sqlite3_step(query.get()) == SQLITE_ROW) {
        result.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0)));
    }
    return result;
}

std::vector<Rule> PersonalStore::rules() const {
    Statement query(db_, "SELECT typo,correction,confidence,previous_word,accepts FROM correction_rules "
                         "UNION ALL SELECT typo,correction,confidence,previous_word,accepts FROM learned_rules "
                         "ORDER BY confidence DESC, accepts DESC;");
    std::vector<Rule> result;
    while (sqlite3_step(query.get()) == SQLITE_ROW) {
        const auto* previous = reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 3));
        result.push_back({reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0)),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 1)),
                          sqlite3_column_double(query.get(), 2),
                          previous && *previous ? std::optional<std::string>(previous) : std::nullopt});
    }
    return result;
}

int PersonalStore::undo_count(const std::string& typo, const std::string& correction) const {
    Statement query(db_, "SELECT coalesce(sum(undos),0) FROM correction_feedback WHERE typo=? AND correction=?;");
    bind(db_, query.get(), 1, lowercase_ru(typo));
    bind(db_, query.get(), 2, lowercase_ru(correction));
    return sqlite3_step(query.get()) == SQLITE_ROW ? sqlite3_column_int(query.get(), 0) : 0;
}

void PersonalStore::blacklist_add(const std::string& app) {
    Statement query(db_, "INSERT OR IGNORE INTO app_blacklist(app_name) VALUES(?);");
    bind(db_, query.get(), 1, app);
    check(sqlite3_step(query.get()), db_, "add app to blacklist");
}

bool PersonalStore::is_app_blacklisted(const std::string& program) const {
    auto normalize = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const auto slash = value.find_last_of("/\\");
        if (slash != std::string::npos) value.erase(0, slash + 1);
        return value;
    };
    const std::string normalized_program = normalize(program);
    for (const auto& configured : blacklist_get()) {
        const std::string app = normalize(configured);
        if (app.empty()) continue;
        if (normalized_program == app) return true;
        // Desktop IDs and Flatpak application IDs commonly add a namespace
        // or a .desktop suffix. Match complete components, never substrings.
        std::size_t offset = 0;
        while (offset <= normalized_program.size()) {
            const auto end = normalized_program.find('.', offset);
            const auto component = normalized_program.substr(offset, end - offset);
            if (component == app) return true;
            if (end == std::string::npos) break;
            offset = end + 1;
        }
    }
    return false;
}

void PersonalStore::blacklist_remove(const std::string& app) {
    Statement query(db_, "DELETE FROM app_blacklist WHERE app_name=?;");
    bind(db_, query.get(), 1, app);
    check(sqlite3_step(query.get()), db_, "remove app from blacklist");
}

std::vector<std::string> PersonalStore::blacklist_get() const {
    Statement query(db_, "SELECT app_name FROM app_blacklist ORDER BY app_name ASC;");
    std::vector<std::string> result;
    while (sqlite3_step(query.get()) == SQLITE_ROW) {
        result.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0)));
    }
    return result;
}

void PersonalStore::set_mode(const std::string& mode) {
    if (mode != "cautious" && mode != "normal" && mode != "active") return;
    Statement query(db_, "INSERT INTO settings(key,value) VALUES('mode',?) "
                         "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
    bind(db_, query.get(), 1, mode);
    check(sqlite3_step(query.get()), db_, "set mode");
}

std::string PersonalStore::mode() const {
    Statement query(db_, "SELECT value FROM settings WHERE key='mode';");
    if (sqlite3_step(query.get()) != SQLITE_ROW) return "normal";
    return reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0));
}

void PersonalStore::set_setting(const std::string& key, bool enabled) {
    Statement query(db_, "INSERT INTO settings(key,value) VALUES(?,?) "
                         "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
    bind(db_, query.get(), 1, key);
    bind(db_, query.get(), 2, enabled ? "1" : "0");
    check(sqlite3_step(query.get()), db_, "set boolean setting");
}

bool PersonalStore::setting_enabled(const std::string& key, bool default_value) const {
    Statement query(db_, "SELECT value FROM settings WHERE key=?;");
    bind(db_, query.get(), 1, key);
    if (sqlite3_step(query.get()) != SQLITE_ROW) return default_value;
    const std::string value = reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0));
    return value == "1" || value == "true" || value == "yes";
}

void PersonalStore::set_string_setting(const std::string& key, const std::string& value) {
    Statement query(db_, "INSERT INTO settings(key,value) VALUES(?,?) "
                         "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
    bind(db_, query.get(), 1, key);
    bind(db_, query.get(), 2, value);
    check(sqlite3_step(query.get()), db_, "set string setting");
}

std::string PersonalStore::string_setting(const std::string& key,
                                          const std::string& default_value) const {
    Statement query(db_, "SELECT value FROM settings WHERE key=?;");
    bind(db_, query.get(), 1, key);
    if (sqlite3_step(query.get()) != SQLITE_ROW) return default_value;
    return reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0));
}

void PersonalStore::add_history(const std::string& original, const std::string& replacement,
                                const std::string& app, const std::string& source) {
    if (original.empty() || replacement.empty() || original == replacement) return;
    Statement query(db_, "INSERT INTO correction_history(original,replacement,app,source) VALUES(?,?,?,?);");
    bind(db_, query.get(), 1, original);
    bind(db_, query.get(), 2, replacement);
    bind(db_, query.get(), 3, app);
    bind(db_, query.get(), 4, source);
    check(sqlite3_step(query.get()), db_, "add correction history");
    exec("DELETE FROM correction_history WHERE id NOT IN "
         "(SELECT id FROM correction_history ORDER BY id DESC LIMIT 100);");
}

void PersonalStore::mark_last_history_undone(const std::string& original,
                                              const std::string& replacement) {
    Statement query(db_, "UPDATE correction_history SET undone=1 WHERE id=(SELECT id FROM correction_history "
                         "WHERE original=? AND replacement=? ORDER BY id DESC LIMIT 1);");
    bind(db_, query.get(), 1, original);
    bind(db_, query.get(), 2, replacement);
    check(sqlite3_step(query.get()), db_, "mark correction undone");
}

std::vector<CorrectionHistoryEntry> PersonalStore::history(int limit) const {
    Statement query(db_, "SELECT id,original,replacement,app,source,undone,created_at "
                         "FROM correction_history ORDER BY id DESC LIMIT ?;");
    check(sqlite3_bind_int(query.get(), 1, std::max(0, limit)), db_, "bind history limit");
    std::vector<CorrectionHistoryEntry> result;
    while (sqlite3_step(query.get()) == SQLITE_ROW) {
        result.push_back({sqlite3_column_int64(query.get(), 0),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 1)),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 2)),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 3)),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 4)),
                          sqlite3_column_int(query.get(), 5) != 0,
                          sqlite3_column_int64(query.get(), 6)});
    }
    return result;
}

void PersonalStore::disable_rule(const std::string& typo, const std::string& correction) {
    Statement query(db_, "INSERT INTO correction_rules(typo,correction,confidence,accepts,undos) "
                         "VALUES(?,?,0,0,1) ON CONFLICT(typo,correction,previous_word) DO UPDATE SET "
                         "confidence=0, undos=undos+1, updated_at=unixepoch();");
    bind(db_, query.get(), 1, lowercase_ru(typo));
    bind(db_, query.get(), 2, lowercase_ru(correction));
    check(sqlite3_step(query.get()), db_, "disable correction rule");
}

bool PersonalStore::is_rule_disabled(const std::string& typo,
                                     const std::string& correction) const {
    Statement query(db_, "SELECT 1 FROM correction_rules WHERE typo=? AND correction=? "
                         "AND confidence<=0 LIMIT 1;");
    bind(db_, query.get(), 1, lowercase_ru(typo));
    bind(db_, query.get(), 2, lowercase_ru(correction));
    return sqlite3_step(query.get()) == SQLITE_ROW;
}

std::vector<Rule> PersonalStore::disabled_rules() const {
    Statement query(db_, "SELECT typo,correction,confidence,previous_word FROM correction_rules "
                         "WHERE confidence<=0 ORDER BY updated_at DESC;");
    std::vector<Rule> result;
    while (sqlite3_step(query.get()) == SQLITE_ROW) {
        const auto* previous = reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 3));
        result.push_back({reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0)),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 1)),
                          sqlite3_column_double(query.get(), 2),
                          previous && *previous ? std::optional<std::string>(previous) : std::nullopt});
    }
    return result;
}

void PersonalStore::enable_rule(const std::string& typo, const std::string& correction) {
    Statement query(db_, "DELETE FROM correction_rules WHERE typo=? AND correction=? AND confidence<=0;");
    bind(db_, query.get(), 1, lowercase_ru(typo));
    bind(db_, query.get(), 2, lowercase_ru(correction));
    check(sqlite3_step(query.get()), db_, "enable correction rule");
}

void PersonalStore::clear_history() {
    exec("DELETE FROM correction_history;");
}

void PersonalStore::reset_learning() {
    exec("BEGIN IMMEDIATE;");
    try {
        exec("DELETE FROM correction_feedback;");
        exec("DELETE FROM word_transitions;");
        exec("DELETE FROM unknown_word_observations;");
        exec("DELETE FROM learned_words;");
        exec("DELETE FROM learned_rules;");
        exec("DELETE FROM correction_history;");
        exec("COMMIT;");
    } catch (...) {
        try { exec("ROLLBACK;"); } catch (...) {}
        throw;
    }
}

void PersonalStore::add_diagnostic(const std::string& original, const std::string& candidate,
                                   const std::string& action, const std::string& reason,
                                   double confidence, const std::string& app) {
    Statement query(db_, "INSERT INTO diagnostics(original,candidate,action,reason,confidence,app) "
                         "VALUES(?,?,?,?,?,?);");
    bind(db_, query.get(), 1, original);
    bind(db_, query.get(), 2, candidate);
    bind(db_, query.get(), 3, action);
    bind(db_, query.get(), 4, reason);
    check(sqlite3_bind_double(query.get(), 5, confidence), db_, "bind diagnostic confidence");
    bind(db_, query.get(), 6, app);
    check(sqlite3_step(query.get()), db_, "add diagnostic");
    exec("DELETE FROM diagnostics WHERE id NOT IN "
         "(SELECT id FROM diagnostics ORDER BY id DESC LIMIT 500);");
}

std::vector<DiagnosticEntry> PersonalStore::diagnostics(int limit) const {
    Statement query(db_, "SELECT original,candidate,action,reason,confidence,app,created_at "
                         "FROM diagnostics ORDER BY id DESC LIMIT ?;");
    check(sqlite3_bind_int(query.get(), 1, std::max(0, limit)), db_, "bind diagnostics limit");
    std::vector<DiagnosticEntry> result;
    while (sqlite3_step(query.get()) == SQLITE_ROW) {
        result.push_back({reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0)),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 1)),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 2)),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 3)),
                          sqlite3_column_double(query.get(), 4),
                          reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 5)),
                          sqlite3_column_int64(query.get(), 6)});
    }
    return result;
}

void PersonalStore::clear_diagnostics() {
    exec("DELETE FROM diagnostics;");
}

PersonalStore::Stats PersonalStore::get_stats() const {
    Stats result;
    {
        Statement query(db_, "SELECT COUNT(*) FROM correction_history;");
        if (sqlite3_step(query.get()) == SQLITE_ROW) {
            result.total_corrections = sqlite3_column_int(query.get(), 0);
        }
    }
    {
        Statement query(db_, "SELECT COUNT(*) FROM correction_history WHERE undone=1;");
        if (sqlite3_step(query.get()) == SQLITE_ROW) {
            result.undone_corrections = sqlite3_column_int(query.get(), 0);
        }
    }
    return result;
}

}  // namespace smarttype
