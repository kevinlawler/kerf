#pragma once
namespace KERF_NAMESPACE {

struct FILE_OPERATIONS
{
  static I open_file_handle(const char* path, bool create = false) noexcept;
  static void* drive_mmap_file_handle(I f, I *file_size, DRIVE_ACCESS_KIND kind = DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE) noexcept;
  static void* drive_mmap_file_path(const char* path, I *file_size, DRIVE_ACCESS_KIND kind = DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE) noexcept;

  static SLOP open_from_drive_path(std::string path, DRIVE_ACCESS_KIND access_kind = DRIVE_ACCESS_KIND_POSIX_READ_FLUSH);
  static SLOP read_from_drive_path(std::string path);
  static SLOP memory_mapped_from_drive_path(std::string path, DRIVE_ACCESS_KIND map_kind = DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE);
  static void write_to_drive_path(const SLOP &y, std::string path, DRIVE_ACCESS_KIND access_kind = DRIVE_ACCESS_KIND_POSIX_WRITE_FLUSH_SINGLEFILE);

  static SLOP open_from_drive_path_directory_expanded(std::string path, DRIVE_ACCESS_KIND access_kind = DRIVE_ACCESS_KIND_POSIX_READ_FLUSH);
  static SLOP open_from_drive_path_multifile_expanded(std::string path, DRIVE_ACCESS_KIND access_kind = DRIVE_ACCESS_KIND_POSIX_READ_FLUSH);
  static SLOP open_from_drive_path_flat_singlefile(std::string path, DRIVE_ACCESS_KIND access_kind = DRIVE_ACCESS_KIND_POSIX_READ_FLUSH);
  static SLOP memory_mapped_from_drive_path_directory_expanded(std::string path, DRIVE_ACCESS_KIND map_kind = DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE);
  static SLOP memory_mapped_from_drive_path_multifile_expanded(std::string path, DRIVE_ACCESS_KIND map_kind = DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE);
  static SLOP memory_mapped_from_drive_path_flat_singlefile(std::string path, DRIVE_ACCESS_KIND map_kind = DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE, bool literal_memory_mapped = false, I offset = 0, bool headerless = false);
  static SLOP read_from_drive_path_directory_expanded(std::string path);
  static SLOP read_from_drive_path_multifile_expanded(std::string path);
  static SLOP read_from_drive_path_flat_singlefile(std::string path);
  static void write_to_drive_path_directory_expanded(const SLOP &x, std::string path, bool writes_self = true, bool is_workspace = false);
  static void write_to_drive_path_multifile_expanded(const SLOP &x, std::string path, bool writes_self = true);
  static void write_to_drive_path_flat_singlefile(const SLOP &x, std::string path);

  static bool ensure_directory_from_path(std::string path, bool path_is_regular_file_not_dir = false, bool remove_existing = false) noexcept;
  static auto write_to_drive_path_header_payload(const SLOP &x, std::string path, bool skip_header = false, bool remove_existing = false, bool allow_literal_memory_mapped = false);

  static void zero_pointers(const SLOP &x);
  static void zero_pointers_at_path(std::string path);

  static std::string workspace_default_path();
  static void workspace_save(std::string path = "");
  static void workspace_load(std::string path = "");
};

struct FILE_REGISTRY
{
  // The purpose of this thing is to dedupe files across the process based on
  // path. To do this we reduce them to inodes on devices. Then when they are
  // deduped we keep a simple per-file rwlock counter protected by a global
  // mutex (sutex). The reason for this is primarily that mmap's MAP_SHARED and
  // MAP_PRIVATE cannot live together simultaneously: whether MAP_SHARED edits
  // appear in a separate MAP_PRIVATE is undefined behavior, and typically
  // edits do---this effectively makes reads inconsistent. Basic POSIX read()
  // and write() calls have similar interactions. You do not want to read() a
  // file into a buffer if another thread is altering it with MAP_SHARED, and
  // so on.

  // We disambiguate all files based on the concatenation st_dev:st_ino from
  // stat(). This handles symlinks, hardlinks, and path equivalence.
  // Only regular files go here, not directories (I think).
  // The reference_counter is keyed on st_dev:st_ino pointing to values
  // consisting of read-write counts ∈ {-1,0,1,...}. Like a rwlock, -1
  // indicates write-locked, 0 indicates not-in-use, and 1+ indicates the
  // readers count.
  // MAP_SHARED uses the write lock.
  // MAP_PRIVATE increments the read lock.
  // Regular file reads and writes need the corresponding lock (actually the
  // wrapper we've made).
  // "Lock" is a bit of a misnomer here. We don't expect anything to block on
  // reference counts being modified by this class. We expect to expose only
  // "try" methods that return immediately.
  // This functionality differs slightly from a SUTEX:

  // Assumption. You can not open a file via inode (the OS prevents this for
  // security reasons). So we blithely assume the user will not change file
  // links (re: path->inode connections) under us. This is about as good as it
  // can get. We can store the paths, but we can't open via inode, so we have
  // to hope the inode changes aren't obscured from us. (This applies to files
  // we mmap and munmap.) Additionally, files shouldn't be deleted from under
  // us and such, since we must open() and close() them multiple times with
  // MEMORY_MAPPED. We likely safely detect that condition, but anyway, the
  // point stands. For the most part, don't mess with our file tree from
  // outside the process while it's active inside the process, even if in some
  // cases you can get away with rewriting symlinks and such if the inodes
  // don't currently have any locks.

  // Assumption. Some file systems may have exotic race conditions with inode
  // and device id reuse. We ignore all these.

  // Regime. For the write lock we've elected to use "-1" instead of
  // -thread_id, as we do with sutex, since the current thinking is that the
  // encapsulating object owns the file, and the process, but not really the
  // thread. So instead of some kind of weird recursion-safety, threads should
  // know what files they have. If we used -thread_id, you could have things
  // like a thread mapping a file for a write, and then mapping it again for a
  // [thread-safe] sub-write, but this seems exotic and weird: I think we would
  // rather have the thread be aware of what it's doing.

  // Commentary. Instead of using `I` for the map's values, we could use SUTEX.
  // Then instead of a new guard it would let you re-use our SUTEX guards and
  // much of that logic. Ultimately however this is much more awkward since you
  // have introspection into the map's values that short-circuits the main
  // global FILE_REGISTRY sutex. It makes several things harder: you need
  // thread-ignoring sutexes, failure-aware sutexes, try-only sutex guards, and
  // will have trouble keeping the global map lightweight, despite that fact
  // that you get guarantees that insert and erase do not move addresses around
  // (Section 23.1.2#8).

  typedef std::string FILE_KEY;
  typedef I FILE_VALUE;

  const bool POP_unset_unused_keys = true;
  const FILE_VALUE NOT_IN_USE_SENTINEL = 0;
  const FILE_VALUE EXCLUSIVE_LOCK_SENTINEL = -1;

  SUTEX sutex = {0};
  std::map<FILE_KEY, FILE_VALUE> refs;

  static FILE_REGISTRY& singleton();

  ~FILE_REGISTRY()
  {
    if(POP_unset_unused_keys)
    {
    #if DEBUG
      if(!refs.empty()) std::cerr <<  "Expected FILE_REGISTRY refs.empty() but aren't.\n";
      if(!The_Did_Interrupt_Flag) assert(refs.empty());
    #endif
    }

    for (auto const& [key, val] : refs)
    {
    #if DEBUG
      if(val!=NOT_IN_USE_SENTINEL) std::cerr <<  "Expected FILE_REGISTRY val==NOT_IN_USE_SENTINEL but was unequal.\n";
      if(!The_Did_Interrupt_Flag) assert(val==NOT_IN_USE_SENTINEL);
    #endif
    }
  }

  struct FILE_REGISTRY_GUARD : REGISTERED_FOR_LONGJMP_WORKSTACK
  {
    bool failed = true;
    std::string path = "";
    bool is_filewrite = true;
    bool use_filecreate = false;

    FILE_REGISTRY_GUARD() {};
    FILE_REGISTRY_GUARD(std::string path, bool is_filewrite, bool use_filecreate) : path(path), is_filewrite(is_filewrite), use_filecreate(use_filecreate)
    {
      if(path != "") failed = singleton().register_path_use(path, is_filewrite, use_filecreate);
    }

    ~FILE_REGISTRY_GUARD()
    {
      if(path != "" && !failed) singleton().deregister_path_use(path);
    }
  };

  static std::string key_for_filepath(std::string path, bool use_filecreate = false)
  {
    auto FAIL = "";
    struct stat buffer;
    int status;

    // status = stat(cpath, &buffer);

    const char *cpath = path.c_str();

    I f = FILE_OPERATIONS::open_file_handle(cpath, use_filecreate);
    if(0 > f)
    {
      perror("File registry string key for filepath: open file failed");
      return FAIL; // NB. this is used in critical sections, so do not ERROR/jmp here
    }

    status = fstat(f, &buffer);

    // Feature: ctrl-c unsafe: protect on stack or file handle guard or critical section or ...
    close(f);

    if(status)
    {
      perror("File path stat() failed");
      std::cerr << "path: \"" << (path) << "\"\n";
      return FAIL;
    }
    std::string separator = ":";
    std::string di = std::to_string(buffer.st_dev) + separator + std::to_string(buffer.st_ino);
    return di;
  }

  static bool drive_access_kind_is_filewrite(DRIVE_ACCESS_KIND kind)
  {
    switch(kind)
    {
      default:
      case DRIVE_ACCESS_KIND_WRITE_WORKSPACE:
      case DRIVE_ACCESS_KIND_WRITE_DIRECTORY:
      case DRIVE_ACCESS_KIND_WRITE_MULTIFILE:
      case DRIVE_ACCESS_KIND_POSIX_WRITE_FLUSH_SINGLEFILE:
      case DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE: // true filewrite, false filecreate
        return true;
      case DRIVE_ACCESS_KIND_READ_WORKSPACE:
      case DRIVE_ACCESS_KIND_POSIX_READ_FLUSH:
      case DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY:
      case DRIVE_ACCESS_KIND_MMAP_PRIVATE_WRITEABLE: // sic, MAP_PRIVATE & PROT_WRITE does not persist to the underlying drive file
        return false;
    }
  }

  static bool drive_access_kind_use_filecreate(DRIVE_ACCESS_KIND kind)
  {
    switch(kind)
    {
      default:
      case DRIVE_ACCESS_KIND_WRITE_WORKSPACE:
      case DRIVE_ACCESS_KIND_WRITE_DIRECTORY:
      case DRIVE_ACCESS_KIND_WRITE_MULTIFILE:
      case DRIVE_ACCESS_KIND_POSIX_WRITE_FLUSH_SINGLEFILE:
        return true;
      case DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE:  // false filecreate, true filewrite
      case DRIVE_ACCESS_KIND_READ_WORKSPACE:
      case DRIVE_ACCESS_KIND_POSIX_READ_FLUSH:
      case DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY:
      case DRIVE_ACCESS_KIND_MMAP_PRIVATE_WRITEABLE: // sic, MAP_PRIVATE & PROT_WRITE does not persist to the underlying drive file
        return false;
    }
  }

  bool register_path_use(std::string path, DRIVE_ACCESS_KIND kind)
  {
    return register_path_use(path, drive_access_kind_is_filewrite(kind), drive_access_kind_use_filecreate(kind));
  }

  bool register_path_use(std::string path, bool is_filewrite = true, bool use_filecreate = false)
  {
    bool SUCCEED = false;
    bool FAIL = !SUCCEED;

    auto key = key_for_filepath(path, use_filecreate);
    if("" == key) return FAIL;

    auto e = sutex.sutex_safe_wrapper_exclusive([&]{
      if(!refs.contains(key))
      {
        refs[key] = NOT_IN_USE_SENTINEL;
      }

      auto v = refs[key];

      if(is_filewrite)
      {
        if(v != NOT_IN_USE_SENTINEL)
        {
          std::cerr << "Could not acquire exclusive file lock for path: \"" << (path) << "\"\n";
          std::cerr << "Lock already held: " << (EXCLUSIVE_LOCK_SENTINEL==v ? "exclusive":"shared")  << "\n";
          return FAIL;
        }
        refs[key] = EXCLUSIVE_LOCK_SENTINEL;
      }
      else
      {
        if(v == EXCLUSIVE_LOCK_SENTINEL)
        {
          std::cerr << "Could not acquire shared file lock for path: \"" << (path) << "\"\n";
          return FAIL;
        }
        refs[key] = v+1; // increment shared lock
      }
      return SUCCEED;
    });

    if(e==FAIL) return FAIL;

    return SUCCEED;
  }

  void deregister_path_use(std::string path) noexcept
  {
    auto key = key_for_filepath(path);
    if("" == key) return;

    sutex.sutex_safe_wrapper_exclusive([&]{
      if(!refs.contains(key))
      {
        return;
      }

      auto v = refs[key];

      if(v == NOT_IN_USE_SENTINEL)
      {
        std::cerr << "Attempted to deregister file lock with nothing locked. Is this following a ctrl-c?\n";
        std::cerr << "path: \"" << (path) << "\"\n";
      }

      if(v == EXCLUSIVE_LOCK_SENTINEL)
      {
        refs[key] = NOT_IN_USE_SENTINEL;
      }
      else
      {
        refs[key] = v-1; // decrement shared lock
      }

      if(POP_unset_unused_keys)
      {
        if(refs[key] == NOT_IN_USE_SENTINEL)
        {
          refs.erase(key);
        }
      }
    });
  }

};

FILE_REGISTRY The_File_Registry;

FILE_REGISTRY& FILE_REGISTRY::singleton(){ return The_File_Registry; }

template<typename L> auto file_rw_safe_wrapper(L &&lambda, std::string path, const bool exclusive = true);
template<typename L> auto file_rw_safe_wrapper(L &&lambda, std::string path, DRIVE_ACCESS_KIND kind);


} // namespace
