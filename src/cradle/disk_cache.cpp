#include <cradle/disk_cache.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

#include <sqlite3.h>

#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#include <accctrl.h>
#include <aclapi.h>
#endif

#include <cradle/fs/app_dirs.hpp>

namespace cradle {

struct disk_cache_impl
{
    file_path dir;

    sqlite3* db = nullptr;

    int64_t size_limit;

    // used to track when we need to check if the cache is too big
    int64_t bytes_inserted_since_last_sweep;

    // list of IDs that whose usage needs to be recorded
    std::vector<int64_t> usage_record_buffer;

    boost::posix_time::ptime latest_activity;

    // protects all access to the cache
    boost::mutex mutex;
};

// SQLITE UTILITIES

void static
open_db(sqlite3** db, file_path const& file)
{
    if (sqlite3_open(file.string().c_str(), db) != SQLITE_OK)
    {
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(file.parent_path()) <<
                internal_error_message_info("failed to create disk cache index file (index.db)"));
    }
}

string static
escape_string(string const& s)
{
    return boost::replace_all_copy(s, "'", "''");
}

void static
throw_query_error(disk_cache_impl const& cache, string const& sql, string const& error)
{
    CRADLE_THROW(
        disk_cache_failure() <<
            disk_cache_path_info(cache.dir) <<
            internal_error_message_info(
                "error executing SQL query in index.db\n"
                "SQL query: " + sql + "\n" +
                "error: " + error));
}

string static
copy_and_free_message(char* msg)
{
    if (msg)
    {
        string s = msg;
        sqlite3_free(msg);
        return s;
    }
    else
        return "";
}

void static
exec_sql(disk_cache_impl const& cache, string const& sql)
{
    char *msg;
    int code = sqlite3_exec(cache.db, sql.c_str(), 0, 0, &msg);
    string error = copy_and_free_message(msg);
    if (code != SQLITE_OK)
        throw_query_error(cache, sql, error);
}

struct db_transaction
{
    db_transaction(disk_cache_impl& cache)
      : cache_(&cache), committed_(false)
    {
        exec_sql(*cache_, "begin transaction;");
    }
    void commit()
    {
        exec_sql(*cache_, "commit transaction;");
        committed_ = true;
    }
    ~db_transaction()
    {
        if (!committed_)
            exec_sql(*cache_, "rollback transaction;");
    }
    disk_cache_impl* cache_;
    bool committed_;
};

// QUERIES

// Get the total size of all entries in the cache.
struct size_result
{
    int64_t size;
    bool success;
};
int static
size_callback(void* data, int n_columns, char** columns, char** types)
{
    if (n_columns != 1)
        return -1;

    size_result* r = reinterpret_cast<size_result*>(data);

    if (!columns[0])
    {
        // Apparently if there are no entries, it triggers this case.
        r->size = 0;
        r->success = true;
        return 0;
    }

    try
    {
        r->size = lexical_cast<int64_t>(columns[0]);
        r->success = true;
        return 0;
    }
    catch (...)
    {
        return -1;
    }
}
int64_t static
get_cache_size(disk_cache_impl& cache)
{
    size_result result;
    result.success = false;

    string sql = "select sum(size) from entries;";

    char* msg;
    int code = sqlite3_exec(cache.db, sql.c_str(), size_callback, &result, &msg);
    string error = copy_and_free_message(msg);
    if (code != SQLITE_OK)
        throw_query_error(cache, sql, error);

    if (!result.success)
        throw_query_error(cache, sql, "no result");

    return result.size;
}

// Get the total number of valid entries in the cache.
struct entry_count_result
{
    int64_t n_entries;
    bool success;
};
int static
entry_count_callback(void* data, int n_columns, char** columns, char** types)
{
    if (n_columns != 1 || !columns[0])
        return -1;

    entry_count_result* r = reinterpret_cast<entry_count_result*>(data);

    try
    {
        r->n_entries = lexical_cast<int64_t>(columns[0]);
        r->success = true;
        return 0;
    }
    catch (...)
    {
        return -1;
    }
}
int64_t static
get_cache_entry_count(disk_cache_impl& cache)
{
    entry_count_result result;
    result.success = false;

    string sql = "select count(id) from entries where valid = 1;";

    char* msg;
    int code = sqlite3_exec(cache.db, sql.c_str(), entry_count_callback,
        &result, &msg);
    string error = copy_and_free_message(msg);
    if (code != SQLITE_OK)
        throw_query_error(cache, sql, error);

    if (!result.success)
        throw_query_error(cache, sql, "no result");

    return result.n_entries;
}

// Get a list of entries in the cache.
int static
cache_entries_callback(void* data, int n_columns, char** columns, char** types)
{
    if (n_columns != 5)
        return -1;

    auto r = reinterpret_cast<std::vector<disk_cache_entry>*>(data);

    try
    {
        disk_cache_entry e;
        e.key = columns[0];
        e.id = lexical_cast<int64_t>(columns[1]);
        e.in_db = columns[2] && lexical_cast<int>(columns[2]) != 0;
        e.size = columns[3] ? lexical_cast<int64_t>(columns[3]) : 0;
        e.crc32 = columns[4] ? lexical_cast<uint32_t>(columns[4]) : 0;
        r->push_back(e);
        return 0;
    }
    catch (...)
    {
        return -1;
    }
}
std::vector<disk_cache_entry> static
get_entry_list(disk_cache_impl& cache)
{
    std::vector<disk_cache_entry> entries;

    string sql =
        "select key, id, in_db, size, crc32 from entries where valid = 1 order by last_accessed;";

    char* msg;
    int code = sqlite3_exec(cache.db, sql.c_str(), cache_entries_callback, &entries, &msg);
    string error = copy_and_free_message(msg);
    if (code != SQLITE_OK)
        throw_query_error(cache, sql, error);

    return entries;
}

// Get a list of entries in the cache in LRU order.
struct lru_entry
{
    int64_t id, size;
    bool in_db;
};
typedef std::vector<lru_entry> lru_entry_list;
int static
lru_entries_callback(void* data, int n_columns, char** columns, char** types)
{
    if (n_columns != 3)
        return -1;

    lru_entry_list* r = reinterpret_cast<lru_entry_list*>(data);

    try
    {
        lru_entry e;
        e.id = lexical_cast<int64_t>(columns[0]);
        e.size = columns[1] ? lexical_cast<int64_t>(columns[1]) : 0;
        e.in_db = columns[2] && lexical_cast<int>(columns[2]) != 0;
        r->push_back(e);
        return 0;
    }
    catch (...)
    {
        return -1;
    }
}
lru_entry_list static
get_lru_entries(disk_cache_impl& cache)
{
    lru_entry_list entries;

    string sql =
        "select id, size, in_db from entries order by valid, last_accessed;";

    char* msg;
    int code = sqlite3_exec(cache.db, sql.c_str(), lru_entries_callback, &entries, &msg);
    string error = copy_and_free_message(msg);
    if (code != SQLITE_OK)
        throw_query_error(cache, sql, error);

    return entries;
}

// Get the entry associated with a particular key (if any).
struct look_up_result
{
    bool exists;
    int64_t id;
    bool valid;
    bool in_db;
    optional<string> value;
    int64_t size;
    uint32_t crc32;
};
int static
look_up_callback(void* data, int n_columns, char** columns, char** types)
{
    if (n_columns != 6)
        return -1;

    look_up_result* r = reinterpret_cast<look_up_result*>(data);

    try
    {
        r->id = lexical_cast<int64_t>(columns[0]);
        r->valid = lexical_cast<int>(columns[1]) != 0;
        r->in_db = columns[2] && lexical_cast<int>(columns[2]) != 0;
        r->value = columns[3] ? some(string(columns[3])) : none;
        r->size = columns[4] ? lexical_cast<int64_t>(columns[4]) : 0;
        r->crc32 = columns[5] ? lexical_cast<uint32_t>(columns[5]) : 0;
        r->exists = true;
        return 0;
    }
    catch (...)
    {
        return -1;
    }
}
optional<disk_cache_entry> static
look_up(
    disk_cache_impl const& cache,
    string const& key,
    bool only_if_valid)
{
    string sql =
        "select id, valid, in_db, value, size, crc32 from entries where key='" +
        escape_string(key) + "';";

    look_up_result result;
    result.exists = false;
    char* msg;
    int code = sqlite3_exec(cache.db, sql.c_str(), look_up_callback, &result, &msg);
    string error = copy_and_free_message(msg);
    if (code != SQLITE_OK)
        throw_query_error(cache, sql, error);

    if (result.exists && (!only_if_valid || result.valid))
    {
        return
            some(
                make_disk_cache_entry(
                    key,
                    result.id,
                    result.in_db,
                    result.value,
                    result.size,
                    result.crc32));
    }
    else
        return none;
}

// OTHER UTILITIES

file_path static
get_path_for_id(disk_cache_impl& cache, int64_t id)
{
    return cache.dir / lexical_cast<string>(id);
}

void static
remove_entry(disk_cache_impl& cache, int64_t id, bool remove_file = true)
{
    file_path path = get_path_for_id(cache, id);
    if (remove_file && exists(path))
        remove(path);

    string sql = "delete from entries where id=" + lexical_cast<string>(id) + ";";

    char* msg;
    int code = sqlite3_exec(cache.db, sql.c_str(), 0, 0, &msg);
    string error = copy_and_free_message(msg);
    if (code != SQLITE_OK)
        throw_query_error(cache, sql, error);
}

void static
enforce_cache_size_limit(disk_cache_impl& cache)
{
    try
    {
        int64_t size = get_cache_size(cache);
        if (size > cache.size_limit)
        {
            auto lru_entries = get_lru_entries(cache);
            std::vector<lru_entry>::const_iterator
                i = lru_entries.begin(),
                end_i = lru_entries.end();
            while (size > cache.size_limit && i != end_i)
            {
                try
                {
                    remove_entry(cache, i->id, !i->in_db);
                    size -= i->size;
                    ++i;
                }
                catch (...)
                {
                }
            }
        }
        cache.bytes_inserted_since_last_sweep = 0;
    }
    catch (...)
    {
    }
}

void static
record_activity(disk_cache_impl& cache)
{
    cache.latest_activity = boost::posix_time::microsec_clock::local_time();
}

void static
record_usage_to_db(disk_cache_impl const& cache, int64_t id)
{
    exec_sql(cache,
        "update entries set last_accessed=strftime('%Y-%m-%d %H:%M:%f', 'now') where id=" +
        lexical_cast<string>(id) + ";");
}

void static
write_usage_records(disk_cache_impl& cache)
{
    db_transaction t(cache);
    for (auto const& record : cache.usage_record_buffer)
        record_usage_to_db(cache, record);
    t.commit();
    cache.usage_record_buffer.clear();
}

void
record_cache_growth(disk_cache_impl& cache, size_t size)
{
    cache.bytes_inserted_since_last_sweep += size;
    // Allow the cache to write out roughly 1% of its capacity between size checks.
    // (So it could exceed its limit slightly, but only temporarily, and not by much.)
    if (cache.bytes_inserted_since_last_sweep > cache.size_limit / 0x80)
        enforce_cache_size_limit(cache);
}

void static
initialize(disk_cache_impl& cache, disk_cache_config const& config)
{
    cache.db = 0;

    cache.dir = config.directory ? *config.directory : get_shared_cache_dir(none, "cradle");

    cache.size_limit = config.size_limit;

    cache.bytes_inserted_since_last_sweep = 0;

    open_db(&cache.db, cache.dir / "index.db");

    exec_sql(cache,
        "create table if not exists entries(\n"
        "   id integer primary key,\n"
        "   key text unique not null,\n"
        "   valid boolean not null,\n"
        "   last_accessed datetime,\n"
        "   in_db boolean,\n"
        "   value blob,\n"
        "   size integer,\n"
        "   crc32 integer);");

    exec_sql(cache,
        "pragma synchronous = off;");
    exec_sql(cache,
        "pragma locking_mode = exclusive;");
    exec_sql(cache,
        "pragma journal_mode = memory;");

    record_activity(cache);

    enforce_cache_size_limit(cache);
}

void static
shut_down(disk_cache_impl& cache)
{
    if (cache.db)
    {
        sqlite3_close(cache.db);
        cache.db = nullptr;
    }
}

void static
check_initialization(disk_cache_impl& cache)
{
    if (!cache.db)
    {
        CRADLE_THROW(disk_cache_uninitialized());
    }
}

// API

disk_cache::disk_cache()
{
    this->impl_ = new disk_cache_impl;
}

disk_cache::disk_cache(disk_cache_config const& config)
{
    this->impl_ = new disk_cache_impl;
    this->reset(config);
}

disk_cache::~disk_cache()
{
    {
        auto& cache = *this->impl_;
        boost::lock_guard<boost::mutex> lock(cache.mutex);
        shut_down(*this->impl_);
    }
    delete this->impl_;
}

void
disk_cache::reset(disk_cache_config const& config)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    shut_down(cache);
    initialize(cache, config);
}

// Reset the cache to an uninitialized state.
void
disk_cache::reset()
{
    shut_down(*impl_);
}

bool
disk_cache::is_initialized()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);

    return cache.db != nullptr;
}

disk_cache_info
disk_cache::get_summary_info()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    // Note that these are actually inconsistent since the size includes
    // invalid entries, while the entry count does not, but I think that's
    // reasonable behavior and in any case not a big deal.
    disk_cache_info info;
    info.directory = cache.dir.string<string>();
    info.entry_count = get_cache_entry_count(cache);
    info.total_size = get_cache_size(cache);
    return info;
}

std::vector<disk_cache_entry>
disk_cache::get_entry_list()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    return cradle::get_entry_list(cache);
}

void
disk_cache::remove_entry(int64_t id)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    cradle::remove_entry(cache, id);
}

void
disk_cache::clear()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    for (auto const& entry : get_lru_entries(cache))
    {
        try
        {
            cradle::remove_entry(cache, entry.id, !entry.in_db);
        }
        catch (...)
        {
        }
    }
}

optional<disk_cache_entry>
disk_cache::find(string const& key)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    record_activity(cache);

    return look_up(cache, key, true);
}

void
disk_cache::insert(string const& key, string const& value)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    record_activity(cache);

    auto entry = look_up(cache, key, false);
    if (entry)
    {
        exec_sql(cache,
            "update entries set valid=1, in_db=1,"
            " size=" + lexical_cast<string>(value.size()) + ","
            " value=\"" + escape_string(value) + "\","
            " last_accessed=strftime('%Y-%m-%d %H:%M:%f', 'now')"
            " where id=" + lexical_cast<string>(entry->id) + ";");
    }
    else
    {
        exec_sql(cache,
            "insert into entries(key, valid, in_db, size, value, last_accessed) values("
            "\"" + escape_string(key) + "\", 1, 1, " +
            lexical_cast<string>(value.size()) + ", "
            "\"" + escape_string(value) + "\", "
            "strftime('%Y-%m-%d %H:%M:%f', 'now'));");
    }

    record_cache_growth(cache, value.size());
}

int64_t
disk_cache::initiate_insert(string const& key)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    record_activity(cache);

    auto entry = look_up(cache, key, false);
    if (entry)
        return entry->id;

    exec_sql(cache,
        "insert into entries(key, valid, in_db) values ('" + escape_string(key) + "', 0, 0);");

    // Get the ID that was inserted.
    entry = look_up(cache, key, false);
    if (!entry)
    {
        // Since we checked that the insert succceeded, we really shouldn't
        // get here.
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(cache.dir) <<
                internal_error_message_info("failed to create entry in index.db"));
    }

    return entry->id;
}

void
disk_cache::finish_insert(int64_t id, uint32_t crc32)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    record_activity(cache);

    int64_t size = file_size(cradle::get_path_for_id(cache, id));

    exec_sql(cache,
        "update entries set valid=1, in_db=0,"
        " size=" + lexical_cast<string>(size) + ","
        " crc32=" + lexical_cast<string>(crc32) + ","
        " last_accessed=strftime('%Y-%m-%d %H:%M:%f', 'now')"
        " where id=" + lexical_cast<string>(id) + ";");

    record_cache_growth(cache, size);
}

file_path
disk_cache::get_path_for_id(int64_t id)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    return cradle::get_path_for_id(cache, id);
}

void
disk_cache::record_usage(int64_t id)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    cache.usage_record_buffer.push_back(id);
}

void
disk_cache::write_usage_records()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    cradle::write_usage_records(cache);
}

void
disk_cache::do_idle_processing()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    if (!cache.usage_record_buffer.empty() &&
        (boost::posix_time::microsec_clock::local_time() -
            cache.latest_activity).total_milliseconds() > 1000)
    {
        cradle::write_usage_records(cache);
    }
}

}
