#pragma once
namespace KERF_NAMESPACE {

I FILE_OPERATIONS::open_file_handle(const char* path, bool create) noexcept
{
  // NB. EISDIR error: O_RDRW on directories; or O_CREAT without O_DIRECTORY on directories
  int attr = O_RDWR; 
  int perms = 0666;

  char* resolved = NULL;
  const char* use = path;

  I f = -1;

  if(create)
  {
    attr |= O_CREAT;
  }

  // This strange looking method is significantly faster
  // than simply always using realpath

  errno = 0;
  f = open(path, attr, perms | O_NOFOLLOW); // fail ELOOP on symlinks

  if(f > 0) goto succeed;

  errno = 0;
  resolved = realpath(path, NULL);

  if(resolved)
  {
    use = resolved;
  }
  else
  {
    switch(errno)
    {
      case ENOENT: // file does not exist
        if(!create) goto fail;
        use = path;
        break;
      default:
        goto fail;
        break;
    }
  }

  f = open(use, attr, perms);

succeed:
  if(resolved) free(resolved);
  if(-1 == fcntl(f, F_SETFD, FD_CLOEXEC)) //this fixes a problem with reset() under DEBUG in the leak-tracker
  {
    // Note. `fork` is disallowed in kerf2, so this is relevant to reset()/execvp
    perror("File could not set close-on-exec");
    kerr() << "close-on-exec path: " << "\"" << (path) << "\"\n";
    // Remark. We'd prefer to use O_CLOEXEC in the open to avoid a minor race condition in a multithreaded context where someone calls reset(). This will never, ever matter. The consequence is a leaked file handle.
  }
  return f;

fail:
  if(resolved) free(resolved);
  return -1;
}

void* FILE_OPERATIONS::drive_mmap_file_handle(I file_handle, I *file_size, DRIVE_ACCESS_KIND kind) noexcept
{
  SLAB* z = nullptr;

  // Question. Is running fstat here a race condition [in the MAP_SHARED case]? Consider the case of multiple MAP_SHARED mappings, on separate threads, which expand and contract files. Answer. Currently, multiple MAP_SHARED mappings are disallowed by the FILE_REGISTRY

  struct stat c;
  I size = -1;
  if(0 > fstat(file_handle,&c))
  {
    perror("File status failed");
    return nullptr; // NB. this is used in critical sections, so do not ERROR/jmp here
  }

  size = c.st_size;

  // POTENTIAL_OPTIMIZATION_POINT
  // here and in other uses of mmap(), OSX (but not Linux currently) allows MAP_NOCACHE
  // additionally, MAP_NORESERVE

  auto prot = PROT_READ | PROT_WRITE;
  auto map = MAP_SHARED | MAP_PRIVATE;

  switch(kind)
  {
    case DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE:
      prot = PROT_READ | PROT_WRITE;
      map = MAP_SHARED;
      break;
    case DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY:
      prot = PROT_READ;
      map = MAP_PRIVATE;
      break;
    case DRIVE_ACCESS_KIND_MMAP_PRIVATE_WRITEABLE:
      prot = PROT_READ | PROT_WRITE;
      map = MAP_PRIVATE;
      break;
    default:
      die( Undefined drive access kind);
  }

  z = (SLAB*) mmap(nullptr, size, prot, map, file_handle, 0);

  if(MAP_FAILED == z)
  {
    perror("File map failed");
    return nullptr; // NB. this is used in critical sections, so do not ERROR/jmp here
  }

  if(file_size) *file_size = size;
  The_Mmap_Total_Byte_Counter += size;

  if(DEBUG)
  {
    // TODO optionally verify mapped file sanity here (makes map non-instantaneous so we don't do by default)
    // for example:
    // assert drive attribute set RECURSIVELY
    // assert z layout() refcount == 1. but i don't think we need that?
    // assert compiled refcount == 0. same
  }

  return z;
}

void* FILE_OPERATIONS::drive_mmap_file_path(const char* path, I *file_size, DRIVE_ACCESS_KIND kind) noexcept
{
  bool create = false;

  I f = FILE_OPERATIONS::open_file_handle(path, create);

  if(0 > f)
  {
    perror("Drive mmap file path failed: Open file failed");
    kerr() << "Path: \"" << (path) << "\"\n";
    return nullptr; // NB. this is used in critical sections, so do not ERROR/jmp here
  }

  auto v = drive_mmap_file_handle(f, file_size, kind);

  // Feature: ctrl-c unsafe: protect on stack or file handle guard or critical section or ...
  close(f);

  return v;
}

#pragma mark -

template<typename L> auto file_rw_safe_wrapper(L &&lambda, std::string path, const bool exclusive, const bool filecreate)
{
  const bool SUCCEED = false;
  const bool FAIL = !SUCCEED;

  FILE_REGISTRY::FILE_REGISTRY_GUARD guard;

  critical_section_wrapper([&]{
    guard = FILE_REGISTRY::FILE_REGISTRY_GUARD(path, exclusive, filecreate);
  });

  if(guard.failed) return FAIL;

  // guard frees at this context/scope end

  return lambda();
}

template<typename L> auto file_rw_safe_wrapper(L &&lambda, std::string path, DRIVE_ACCESS_KIND kind)
{
  return file_rw_safe_wrapper(lambda, path, FILE_REGISTRY::drive_access_kind_is_filewrite(kind), FILE_REGISTRY::drive_access_kind_use_filecreate(kind));
}

#pragma mark -

bool FILE_OPERATIONS::ensure_directory_from_path(std::string path, bool path_is_regular_file_not_dir, bool remove_existing) noexcept
{
  // NB. We want this boolean so we can use it without causing a longjmp in the wrong place

  const bool SUCCEED = false;
  const bool FAIL = !SUCCEED;

  if(remove_existing)
  {
    // Feature. We could also go looking for the implied multfile siblings and delete those as well. For instance, if we're writing `0/` and deleting the old `0/`, we could delete the old `0|0` and `0|1|2` as well.

    if(std::filesystem::exists(path))
    {
      // Question. Should we delete any existing (terminal) directory first [Alternatively. Any items inside of it]? One of the issues this would address is leftover files. Such files are present when I renamed the extension during development, and would be present for say `101/` or `101.dir/` from a leftover previously written element, when our currently writing object stops at `9/` or `9.dir/` Answer. You have to delete/clean the directory first for this reason. Otherwise a `101/` directory would be attempted to be erroneously applied to our in-progress object. (Alternatively. we could check for this.) Complication. Because of deleting, now you have the issue of file locks? We can just ignore. 
      // Question. Don't we need the write locks on any files we plan to blast [as part of subdirectories]? Answer. Sounds like we're ignoring this. However, we could check path_is_regular_file_not_dir here and acquire the write lock with the FILE_REGISTRY before deleting

      // Consider. the case where you write a regular file, memory map it in A, but overwrite it with a directory and memory map that in B. Then A currently shows an error when it releases because its path won't resolve to a regular-file inode, and the inode it left behind in the FILE_REGISTRY is leaked.
      // Feature. If path points to a regular-file, you can check the FILE_REGISTRY to first try to acquire the lock (then error / delete it and release it). Similarly, if this is a directory, you'd have to recursively look for regular-files. Alternative. Feature. We may be also able to store the device:inode key on the MEMORY_MAPPED instead...what was the reason we avoided that in the first place? I see...seems we stubbed it out but didn't finish it. The idea is that you'd release based on a cached inode key instead of re-looking it up via the path. But you have to look up via the path any time you re-mmap (unless you cache the filehandle, but that is disallowed). So it's not clear that this is actually needed: it's likely better to go looking for locks whenever you delete/recursively delete from inside the app.

      std::error_code ec;
      std::filesystem::remove_all(path, ec); // NB. won't follow symlinks
      if(ec) 
      {
        kerr() << "Could not remove directory: " << ec.message() << "\n";
        kerr() << "Path: \"" << (path) << "\"\n";
        return FAIL;
      }
    }
  }

  std::filesystem::path dpath(path);

  if(path_is_regular_file_not_dir)
  {
    if(!dpath.has_filename())
    {
      std::cerr << "No filename in path: \"" << (dpath) << "\"\n";
      return FAIL;
    }
    dpath.remove_filename();
  }

  std::error_code ec;
  // Assumption. Directory created or exists: OK. Path is regular file: not OK.
  std::filesystem::create_directories(dpath, ec);
  if(ec) 
  {
    kerr() << "Could not create/ensure directory: " << ec.message() << "\n";
    return FAIL;
  }

  return SUCCEED;
}

void FILE_OPERATIONS::zero_pointers_at_path(std::string path)
{
  const SLOP &s = memory_mapped_from_drive_path_flat_singlefile(path, DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE, true);
  A_MEMORY_MAPPED &m = *(A_MEMORY_MAPPED*)s.presented();

  // Factor. I'd rather use `m.rwlocker_wrapper_write([&](auto p){...` but couldn't figure out how to 'include' here
  m.parent()->slabp->sutex.sutex_safe_wrapper_exclusive([&]{
    SLOP h = m.get_good_slop();
    FILE_OPERATIONS::zero_pointers(h);
  });

}

void FILE_OPERATIONS::zero_pointers(const SLOP &x)
{
  if(x.layout()->known_flat()) return;

  auto g = [&](const SLOP& u) { 
    if(u.is_tracking_parent_rlink_of_slab())
    {
      SLAB** t = u.tracking_parent_address();
      // SLAB* store = *t;
      *t = nullptr; // erase pointer on the drive
      // SLOP z(store); // this will work if store is still in RAM
      return;
    }
    zero_pointers(u);
  };

  x.iterator_uniplex_layout_subslop(g);
}

#pragma mark - 

auto FILE_OPERATIONS::write_to_drive_path_header_payload(const SLOP &x, std::string path, bool skip_header, bool remove_existing, bool allow_literal_memory_mapped)
{
  // Compare SLOP::force_copy_slab_to_ram

  // NB. This writes RLINK3s raw
  // NB. This will write, eg, a 10-byte vector to the drive, as opposed to a 16-byte padded vector for "alignment". I think this is OK but we may change our minds on this. Consequently, if ragged is not acceptable, then padding to MINIMAL_ALIGN may be necessary, or even to PAGE_SIZE_BYTES. 

  // Theory. For file writes, the way we've set things up is, the FILE_REGISTRY
  // more or less is holding "locks" for the MEMORY_MAPPED objects [as opposed
  // to threads or the kind of ad hoc writes we're doing here]. [The key idea
  // is that a MEMORY_MAPPED may interact with multiple threads. So the
  // ownership of the file cannot be limited to a single thread.] For instance,
  // we lock with a `-1` sentinel instead of a thread id. This implies that
  // threads do not own locks. They cannot recurse on the locks. Also, you do
  // not "wait" on file locks: you throw an error if something goes wrong.
  // Threads must keep track of their interaction with files themselves. Still,
  // we need our one-off "flush" file writes have to play nicely under this
  // system that was not especially designed for them. So we use the wrapper as
  // before, and we've made a few modifications. Because you cannot acquire the
  // [inode-based] lock on a file that does not exist yet, we need to be able
  // to create the files and acquire the locks in a race-condition-free way. So
  // what happens is we pass an O_CREAT flag and then acquire the FILE_REGISTRY
  // lock. This works in the presence of multiple threads competing for the
  // write lock without a file existing. Afterward, we truncate the file to
  // zero before writing. Note that currently, no locks are needed on directory
  // paths [whether a parent of a regular file or an end in itself]: either the
  // directory exists/we create it, or it points to a regular-file/fails and
  // errors. 
  // Regime. What this amounts to currently, is that our concurrency model has
  // error-checking for thread competition over flush-writes (as opposed to
  // MAP_SHARED writes) to the same file path, but not full support for
  // automatically synchronizing ordering [note that you can do this yourself
  // in userland, although in our model some similar things are automatic].
  // This I think is the right choice at this time. There may be OS-level
  // instances where such a thing is necessary, for instance, I believe Linux
  // uses write locks or process ids at a given `/var/x/y/z` path, of some
  // fashion (I may be wrong on the robustness of the model here, which would
  // strengthen my argument), but at the process level, I think it makes more
  // sense to keep flush-write filepaths separate between threads for this
  // reason. The use case is exotic. Open for debate.

  if(!allow_literal_memory_mapped && MEMORY_MAPPED == x.layout()->header_get_presented_type())
  {
    // Attempting to write a literal MEMORY_MAPPED should only be possible from
    // cpp-land, not from kerf-userland (excepting writing workspaces), so
    // we'll try our best to catch it here. Though this should be sound now I
    // doubt it will survive refactorings without special effort.
    kerr() << "Attempted to write literal MEMORY_MAPPED using wrong access kind." << '\n';
    ERROR(ERROR_MEMORY_MAPPED);
  }

  auto errdir = FILE_OPERATIONS::ensure_directory_from_path(path, true, remove_existing);
  if(errdir) ERROR(ERROR_FILE);

  const bool SUCCEED = false;
  const bool FAIL = !SUCCEED;

  // Feature. we can pass O_EXCL (with O_CREAT) via writes to make sure regular-file does not exist beforehand 
  auto lambda = [&] {
    // Assumption. ofstream is supposed to truncate by default
    // Wishlist: We truncate here but it would be nice if OS/filesystem could detect when we were overwriting a sector with the same data speed up the write somehow, this is probably infeasible, but in that case we'd rather truncate at the end of the write than at the start

    std::ofstream o(path.c_str(), std::ios_base::trunc);
    if(!o)
    {
      kerr() << "Write to drive path header ofstream error: " << std::strerror(errno) << '\n';
      return FAIL;
    }

    int magic = 4;
    SLAB s = *x.slabp;

    // HACK. this should be t_slab_object_layout_type dependent, probably better handled by TRANSMITTER
    s.reference_management_arena = REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT;

    switch(x.layout()->header_get_slab_object_layout_type())
    {
      case LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM:
        s.t_slab_object_layout_type = LAYOUT_TYPE_UNCOUNTED_ATOM;
        break;
      case LAYOUT_TYPE_TAPE_HEAD_COUNTED_VECTOR:
        s.t_slab_object_layout_type = LAYOUT_TYPE_COUNTED_VECTOR;
        break;
      default:
        break;
    }

    if(!skip_header)
    {
      // magic
      o.write( (const char*)&s, magic);
      // header
      if(LAYOUT_TYPE_TAPE_HEAD_COUNTED_VECTOR == x.layout()->header_get_slab_object_layout_type())
      o.write(((const char*)x.layout()->header_pointer_begin()) + magic, x.layout()->header_bytes_total_capacity() - magic - sizeof(V));
      else
      o.write(((const char*)x.layout()->header_pointer_begin()) + magic, x.layout()->header_bytes_total_capacity() - magic);
    }

    // payload
    if(LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM == x.layout()->header_get_slab_object_layout_type())
    o.write((const char*)x[0][0].slabp->i, sizeof(SLAB::i));
    else
    o.write( (const char*)x.layout()->payload_pointer_begin(), x.layout()->payload_bytes_current_used());

    o.flush();
    o.close();
    
    return SUCCEED;
  };

  auto result = file_rw_safe_wrapper(lambda, path, DRIVE_ACCESS_KIND_POSIX_WRITE_FLUSH_SINGLEFILE);
  if(result)
  {
    ERROR(ERROR_FILE);
  }

}

void FILE_OPERATIONS::write_to_drive_path(const SLOP &y, std::string path, DRIVE_ACCESS_KIND access_kind)
{
  // POP We can of course switch to multifile at the end (leaves) of a directory-themed write, but
  // another thing we can do is instead of writing
  //
  // mydir/
  // midir/0/       [directory]
  // midir/0/_self  [regular file]
  // midir/1/       [directory]
  // midir/1/_self  [regular file]
  //
  // We can write 
  // 
  // mydir/
  // midir/0  [a regular file [or even multifile]]
  // midir/1  [a regular file [or even multifile]]
  //
  // And the recursive read will detect it. What this does is save us a useless
  // last level of directories. I think the way you would detect this is if
  // seeing if a [putative] leaf is known_flat and then just writing it as a
  // regular file instead of a directory with a single base regular file.
  // We could even make separate name for this: DIRECTORY_FLAT_LEAF
  // This gives us an idea for multifile optimization near the leaves: detect
  // desired remaining depth and/or deduced number of files it would add to the
  // current directory.
 
  switch(access_kind)
  {
    case DRIVE_ACCESS_KIND_POSIX_WRITE_FLUSH_SINGLEFILE:
      return FILE_OPERATIONS::write_to_drive_path_flat_singlefile(y, path);
    case DRIVE_ACCESS_KIND_WRITE_WORKSPACE:
      return FILE_OPERATIONS::write_to_drive_path_directory_expanded(y, path, true, true);
    case DRIVE_ACCESS_KIND_WRITE_DIRECTORY:
      return FILE_OPERATIONS::write_to_drive_path_directory_expanded(y, path, true, false);
    case DRIVE_ACCESS_KIND_WRITE_MULTIFILE:
      return FILE_OPERATIONS::write_to_drive_path_multifile_expanded(y, path);
    default:
      die(Undefined drive access kind for write_to_drive_path)
  }
}

void FILE_OPERATIONS::write_to_drive_path_flat_singlefile(const SLOP &x, std::string path)
{
  SLOP y = x;
  y.coerce_FLAT();
  return FILE_OPERATIONS::write_to_drive_path_header_payload(y, path, false, true, false);
}

void FILE_OPERATIONS::write_to_drive_path_directory_expanded(const SLOP &x, std::string path, bool writes_self, bool is_workspace)
{
  // NB. Two non-flat cases of interest: RLINK3_ARRAY (highly regular) and CHUNK_TYPE_VARSLAB (may have n̶e̶s̶t̶e̶d̶ RLINK inside of non-RLINK children)

  // "."
  // ".."
  // "_self"
  // "_incomplete/"
  // "_parted/"
  // "2/"
  // "3/"
  // "7/7/2/6/"

  const bool tracks_incomplete = false; // optional
  bool zeroes_pointers = true; // This isn't necessary, though it's cleaner (& slower) to store nullptrs in RLINKs on the drive instead of random expiring addresses from RAM

  // Feature. Track depth, then switch to write_to_drive_path(..., DRIVE_ACCESS_KIND_POSIX_WRITE_FLUSH_MULTIFILE) at an appropriate depth, when writes_self is also true.

  auto path_incomplete = path + "/" + "_incomplete";

  if(writes_self)
  {
    // POP after deleting the root directory, no need to delete on subsequent recursion
    auto errdir = FILE_OPERATIONS::ensure_directory_from_path(path, false, true); // Remark. We do need a separate ensure directory here, at the top-level, as opposed to letting the recursion handle it at `write_to_drive_path_header_payload` (which contains a nested ensure directory), because we need to delete extraneous files that may be presented in the directory, which would not be covered by the specific regular-files we intend to write.
    if(errdir) ERROR(ERROR_FILE);

    if(tracks_incomplete)
    {
      // Feature. Idea. Begin with an "_incomplete" a marker inside the directory indicating the directory_expanded write did not complete successfully (remove on finish). (Should be trivially easy. Make the dir at the same place as our only dir-making call. Then delete it at `finish` at the bottom (sink for all return points). )  Feature. Put a placeholder on the non-SLOP workstack and delete the directory if the marker is present Alternative. Instead of a marker, suffix the directory name "._incomplete" or create the directory in a tmp folder and atomically rename it to the desired location, or create the directory with a tmp name `.tmp213728` and rename it atomically.
      
      // Remark. You need a separate dir creation for the _incomplete marker because the original root creation deletes any pre-existing leftovers at the root.
      auto errdir = FILE_OPERATIONS::ensure_directory_from_path(path_incomplete, false, false);
      if(errdir) ERROR(ERROR_FILE);
    }

    if(is_workspace)
    {
      // Alternatively. Could be part of, say, a ".attributes" file that represents a map of attributes.
      auto path_workspace = path + "/" + FILENAME_WORKSPACE_MARKER;
      auto errdir = FILE_OPERATIONS::ensure_directory_from_path(path_workspace, false, false);
      if(errdir) ERROR(ERROR_FILE);
    }

    auto _self_path = path + "/" + FILENAME_DIRECTORY_BASE;
    bool allow_literal_memory_mapped = is_workspace;
    write_to_drive_path_header_payload(x, _self_path, false, false, allow_literal_memory_mapped);

    if(true or is_workspace && x.layout()->header_get_presented_type() == MEMORY_MAPPED)
    {
      //Bug, er, Feature. we can do zero pointers here, but [I think the problem is] you need a different method besides cheating and using a mmap-style drive kind to open MEMORY_MAPPED, which is disallowed
      // Idea. ...you can't zero pointers of something you're writing that's still open? (eg during MAP_SHARED?) same for read as well (you can't overwrite) ... but we should already detect that with the filelock?
      zeroes_pointers = false;
    }

    if(zeroes_pointers) zero_pointers_at_path(_self_path);
  }

  I counter = 0;
  auto g = [&](const SLOP& u) { 

    I my_number = counter++;
    auto down = path + "/" + std::to_string(my_number);

    bool self = false;

    SLOP x = u;

    // The slop tracking_parent item will have the pointer we need, when it exists. eazy peazy. Alternatively. Some of our factorization magic should be, build a method like "follow()" or "RLINK3_follow()" or "descend" on SLOP (a no-op on non-RLINK, but on RLINK follows the link), and an init attribute, I think which already exists, like "not descend", and then you can do x =  SLOP(.) and then x.descend or x.follow  after if you need to.

    if(u.is_tracking_memory_mapped())
    {
      self = true;
      
      if(is_workspace) x = u.self_or_literal_memory_mapped_if_tracked();
    }
    else if(u.is_tracking_parent_rlink_of_slab())
    {
      // keyword BROKEN_CHAIN_RLINK. this is handled
      self = true;
    }
    else if(u.layout()->known_flat())
    {
      return;
    }

    write_to_drive_path_directory_expanded(x, down, self, is_workspace);
  };

  if(x.layout()->known_flat()) goto finish;

  // POP. You could maybe parallelize this
  x.iterator_uniplex_layout_subslop(g);

finish:

  if(tracks_incomplete)
  {
    if(writes_self)
    {
      std::error_code ec;
      std::filesystem::remove_all(path_incomplete, ec);
      if(ec) 
      {
        kerr() << "Could not remove incomplete directory: " << ec.message() << "\n";
        kerr() << "Path: \"" << (path) << "\"\n";
        ERROR(ERROR_FILE);
      }
    }
  }

  return;
}

void FILE_OPERATIONS::write_to_drive_path_multifile_expanded(const SLOP &x, std::string path, bool writes_self)
{
  // Feature. You could track incomplete here, as in directory_expanded, by appending say "-_incomplete" to the filename instead of a number and writing it as an empty regular-file or empty directory.

  // Feature. Remark. You could also easily write a multifile inside of a parent directory, this doesn't need any modifications really, you just have a simple directory ahead of it in the path, then open that, for instance: write multifile at path `/cwd/uniqueparent/_self` (so for instance, this may create `_self|0`, `_self|1`, and so on), then you can read/open/mmap from `/cwd/uniqueparent/` and it will all work. This gets around the issue where certain [user-initiated] multfile writes can collide, if you make careless use of the separator in your filenames (this is more common with something like dash '-', eg two separately user-initiated writes to myfile.dat and myfile-0.dat may collide by reusing the latter.). There is no good way to prevent this, aside from enforcing naming conventions on writes, so this that the user has to mind collisions is an assumption.
  // Assumption. The user has to mind naming collisions, because we can't stop them short of enforcing rules on the naming of writes
  // Feature. You could detect write collisions in advance and error. Alternatively. Detect not-in-advance and throw an error, terminating the colliding write before it does anything damaging.

  // Idea. Here's a funny idea. If we could always be guaranteed to be able to
  // claimed some fixed but arbitrary address in memory (fixed per-OS, but can
  // differ between them) (for instance, using MAP_FIXED at 0xaaaabbbbccccdddd
  // or wherever we can get some kind of guarantee - possibly, without checking
  // now, this guaranteed "fixed" address could live inside the deployed kerf
  // binary's code, which is then mapped to some predictable area in virtual
  // memory), of at least 16 bytes (really, 8), then we can pre-populate that
  // memory address with a valid atom (or compact empty list), and then always
  // use that address as a placeholder pointer that we write, that will never
  // become invalidated between processes launches. Cross-platform consistency
  // is better: If the magic address differs between platforms, however, then
  // you'd have to migrate that value cross platform *when migrating pointer
  // values across the bus, eg drive or inet*. One of the nice things about
  // such a scheme is that it allows us to skip null pointer checks in various
  // iterators, helper code. | One of the big benefits for having this fixed
  // memory thing is if you get interrupted during a read, you're not going to
  // die when you clean that up later (and also don't have to add nullptr or
  // other checks). If you were being really careful about it, you'd do like a
  // critical section pass where you went through and pre-populated everything
  // with a fixed address dummy object before you went through and read things
  // from the drive, and then when you unmapped it or wrote it maybe you'd do a
  // pass then too, I haven't checked, anyway it's more bespoke and slower but
  // more resistant to being interrupted then dying later.

  // "."
  // ".."
  // "myfile.dat"
  // "myfile|2.dat"  (where '|' is whatever the separator is, maybe '-')
  // "myfile|3.dat"
  // "myfile|3|0.dat"
  // "myfile|3|1.dat"
  // "myfile|4|7|2.dat"

  const bool zeroes_pointers = true;

  if(writes_self)
  {
    write_to_drive_path_header_payload(x, path, false, true);
    if(zeroes_pointers) zero_pointers_at_path(path);
  }

  I counter = 0;
  auto g = [&](const SLOP& u) { 

    I my_number = counter++;

    bool self = false;

    SLOP x = u;

    if(u.is_tracking_memory_mapped())
    {
      self = true;
    }
    else if(u.is_tracking_parent_rlink_of_slab())
    {
      // keyword BROKEN_CHAIN_RLINK. this is handled
      self = true;
    }
    else if(u.layout()->known_flat())
    {
      return;
    }

    // convert myfile.dat to myfile-0.dat [modify a copy of `path` and store in `down`]
    // Because we modify the filename "in place" we can recurse directly on it [the path]

    auto fpath = std::filesystem::path(path);
    auto separator = std::filesystem::path(FILENAME_MULTIFILE_SEPARATOR); 
    auto parent = fpath.parent_path();
    auto extension = fpath.extension();
    auto stem = fpath.stem();

    // It's not clear to me why std::filesystem insists on making this ugly vs one-liner
    auto down = parent;
    down /= stem;
    down += separator;
    down += std::to_string(my_number);
    down += extension;

    write_to_drive_path_multifile_expanded(x, down, self);
  };

  if(x.layout()->known_flat()) goto finish;

  // POP. You could maybe parallelize this
  x.iterator_uniplex_layout_subslop(g);

finish:

  return;
}

#pragma mark - 

SLOP FILE_OPERATIONS::open_from_drive_path(std::string path, DRIVE_ACCESS_KIND access_kind)
{
  // Feature. do we want a generic "open" access kind that will, say, detect directories and mmap them, but also heuristically detect small files and flush them to ram?

  std::error_code ec;
  auto s = std::filesystem::status(path, ec); // follows symlinks
  if(ec) 
  {
    kerr() << "Could not open from drive path: " << ec.message() << "\n";
    kerr() << "Path: \"" << (path) << "\"\n";
    ERROR(ERROR_FILE);
  }

  if(std::filesystem::is_directory(s))
  {
    return open_from_drive_path_directory_expanded(path, access_kind);
  }
  else if(!std::filesystem::is_regular_file(s))
  {
    kerr() << "Path is not regular file: \"" << (path) << "\"\n";
    ERROR(ERROR_FILE);
  }

  return open_from_drive_path_multifile_expanded(path, access_kind);
}

SLOP FILE_OPERATIONS::open_from_drive_path_directory_expanded(std::string path, DRIVE_ACCESS_KIND access_kind)
{
  // Remark. you can't further make a "rw_drive_path" method that also accepts WRITE access_kinds because it would change the signature to accept a SLOP

  // Remark. Mulitfile is all handled here because it will be captured by the generic `read_from_drive_path` on the base regular-file

  // 3 cases:
  // Done. A: an unpopulated pointer in base CHUNK_RLINK3 RLINK3_ARRAY looks for another "directory" sibling to the base (it can be non-directory)
  // Done. B: an unpopulated pointer in base CHUNK_VARSLAB RLINK_UNIT looks for another directory sibling to the base
  // Feature. C: an non-flat yet non-RLINK3 CHUNK_VARSLAB needs to recurse into a directory without a base until it finds its n̶e̶s̶t̶e̶d̶ broken-chain RLINK_UNIT. keyword BROKEN_CHAIN_RLINK
  // Question. How do we do this again? haha. Answer. The RLINK3_ARRAY looks in constant time. The VARSLAB...  the directories indicate recursion positions. If constant time, we recurse in constant time access, otherwise, we iterate through the VARSLAB and the directory list simultaneously, then recursing. We need a separate recurse function I think (or signature alteration here) to avoid hunting for base regular file when it should be absent for a non-flat.

  // Remark. (keyword: BROKEN_CHAIN_RLINK) I don't know that we ever strictly disallowed the n̶e̶s̶t̶e̶d̶ broken-chain case [a broken-chain RLINK has a VARSLAB parent, which is NOT an RLINK, between itself and the top-level of the VARSLAB array], and we [can] handle it here. In layout()->append, we did effectively disallow it by forcing coerced flat to append to existing flat, and coerced non-flat to append to existing non-flat. What this [quite usefully] enforces is that either an RLINK is not present or there is an unbroken chain of RLINKs to any RLINK inside the VARSLAB strucuture. (So I suppose we can call them "broken chain RLINKs" instead of nested.) The way forward to support broken-chain RLINKs here is to allow directories *without* populating the base `_self` file but which may contain subdirectories. The non-base-containing directories correspond to non-RLINK parents, and the populated nodes (including equivalents, eg non-dir `0` regular files), correspond to RLINKs. Note that broken chains may connect to broken chains arbitrarily. Note that we should be able to "artificially" construct broken-chain-rlinks-in-varslabs by temporarily fudging the known-flat bit prior to appending UNTYPED_SLAB4_ARRAY to UNTYPED_LIST. Note. You can create a test for broken-chain links and then assert() it at appropriate places, potentially in conjunction with a compiler define, ALLOWS_BROKEN_CHAIN_RLINKS. Note. The write family of functions should already handle the empty directories (see `writes_self` in signature), and then LAYOUT_BASE::cow_append is the other function of concern.

  // Feature. assert path is actually a directory
  // Feature. Detect "_incomplete". See `write_*`

  auto path_workspace = path + "/" + FILENAME_WORKSPACE_MARKER;

  if(std::filesystem::exists(path_workspace) && access_kind != DRIVE_ACCESS_KIND_READ_WORKSPACE  )
  {
    kerr() << "path: \"" << (path_workspace) << "\"\n";
    kerr() << "Warning: attempting to open a workspace with an invalid access kind, defaulting to valid access kind for workspaces." << "\n";
    access_kind = DRIVE_ACCESS_KIND_READ_WORKSPACE;
  }
  
  auto base = path + "/" + FILENAME_DIRECTORY_BASE;
  auto s = open_from_drive_path(base, access_kind);

  if(s.layout()->known_flat()) return s;

  if(DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY == access_kind)
  {
    // The case of (MAP_PRIVATE & ¬PROT_WRITE) is special cased. It needs a
    // modifiable slab in order to populate the missing pointers, and so we
    // choose to give it an in-RAM slab to write on
    auto desired_replacement_kind = DRIVE_ACCESS_KIND_POSIX_READ_FLUSH;
    s = open_from_drive_path(base, desired_replacement_kind);
  }

  ITERATOR_LAYOUT z(s, false, false);
  if(z.known_item_count == 0) return s;

  I replacement_count = 0;

  std::filesystem::directory_options options = (std::filesystem::directory_options::follow_directory_symlink);
  std::error_code ec;
  for(auto const& dir_entry: std::filesystem::directory_iterator(path, options, ec))
  {
    if(ec)
    {
      kerr() << "Directory iterator error: " << ec.message() << "\n";
      kerr() << "Path: \"" << (dir_entry.path()) << "\"\n";
      ERROR(ERROR_FILE);
    }

    auto d = dir_entry.path();
    auto filename = d.filename(); // NB. can be a directory

    I i = strtoll(filename.c_str(), nullptr, 10);

    bool valid_name = (filename == std::to_string(i)); // valid: "0", "1", ... ; invalid: "-0", "01", "-1", "abc", etc.

    if(!valid_name) continue;

    // Feature. We can't check for valid index currently, I think there's an issue with s.count crashing or something, recomputing it is also not performant (for O(n) lists), you'd want to the ITERATOR_LAYOUT seek to end and back and then leverage the known_count
    // bool valid_index = (0 <= i) && (i <= s.count());
    // if(!valid_index) continue;

    // POP for dirs/multifiles [as you try to populate the parent _self], the actual optimal read order is neither going to be iterating from left [in the object] and looking for dirs or iterating through the dirs and looking in the object: because of O(n)-time iterators, the optimal will be pre-making a list of dirs and stepping through the object and the dirs at the same time.
    // POP. This is ideal for O(1) iterating arrays, not ideal for O(n): in that case you'd need to pre-populate a list of directories, sort it ("0/", "1/", ..., "10/", "11/", ...) and then iterate through the O(n) list and the list of dirs at the same time. Note that std::filesystem::directory_iterator does not guarantee order of iteration. POP Pre-populating them and sorting them w̶i̶l̶l̶ should also speed up O(1) access in RLINK3_ARRAY: when we merged the two switch cases (the other being chunk VARSLAB), we lost RLINK3_ARRAY's nice guarantee where we could go looking for the files by name, instead of starting from the files and working backwards.
    // Important Observation. when we rewrite this to pre-populate directories, that's actually how you factor this with multifile. for directories you get the pre-list of directories, for multifile you get the pre-list of regular-files (multifile siblings) and then you treat them in the same way. there is a difference in depth to be concerned about, though. directory files happen at one level deeper.
    z.sideways(i - z.i); // NB. iterating forwards via sideways(positive) or next() is efficient with O(n) access lists

    if(!z.over_rlink_reference()) continue; // this is either a file we should ignore (base:"_self", or maybe `_parted`, etc.) or some kind of drive-format error we could check for | for broken-chain-links, it's where we'd recurse to a directory without a basefile

    auto u = open_from_drive_path(d, access_kind);


    if(access_kind == DRIVE_ACCESS_KIND_READ_WORKSPACE)
    {
      u = u.self_or_literal_memory_mapped_if_tracked();
    }

    critical_section_wrapper([&]{
      u.coerce_parented();
      u.legit_reference_increment();
      *z.rlink_address() = u.layout()->slab_pointer_begin();
      replacement_count++;
    });
  }

  // Remark. Warning. Bug. Seems like there's an issue here (above ^^^) where if we ctrl-c, we could leave an in-progress RLINK3 vector in an intermediate state: first element is filled with a valid pointer but second pointer is null or garbage. So either you leak the valid one or die on the invalid one when freeing etc.

  if(s.layout()->header_get_presented_type() == MEMORY_MAPPED && access_kind == DRIVE_ACCESS_KIND_READ_WORKSPACE)
  {
    if(access_kind != DRIVE_ACCESS_KIND_READ_WORKSPACE)
    {
      // I think?
      kerr() << "Attempted to read literal MEMORY_MAPPED using wrong access kind." << '\n';
      ERROR(ERROR_MEMORY_MAPPED);
    }
    assert(!s.is_tracking_memory_mapped()); // I don't think we want to allow mmapping a MEMORY_MAPPED either (double-link)
    A_MEMORY_MAPPED &e = *(A_MEMORY_MAPPED*)s.presented();

    e.zero_derived();

    auto headerless = e.get_is_headerless();
    auto offset = e.get_offset();
    auto object_path = e.get_path();
    auto object_access_kind = e.get_drive_access_kind();
    auto function_signature_path = path;

    // Regime. Instead of doing a true "dehydrated" MEMORY_MAPPED (ie, an alternate formulation such as a separate presented_type), we're just writing it literally to disk, then reading it back and reconstructing (rehydrating) a *separate* MEMORY_MAPPED in RAM that's using the values that were stored that we read. I think we don't need this for headerless or offset MEMORY_MAPPEDs (which should depend on a singlefile) here (this lets us avoid writing a separate rehydrator path for them, which doesn't sound so bad actually), and that this is for "recursive" (directory) types (which need a full recursive read with pointer populating and such). It would probably be better if we read that kind of thing directly off the object. Note. Writing a MM "re-hydrator" like this may go a long way to solve our VARSLAB rehydration issues with inet

    if(headerless or 0!=offset)
    {
      return s;
    }

    std::filesystem::path fsp(object_path);
    if(fsp.filename() == FILENAME_DIRECTORY_BASE) fsp = fsp.remove_filename(); // HACK. err, technically, this should be based on a MEMORY_MAPPED attribute that was stored on whether it was a directory or not, or something like that, I dunno. MEMORY_MAPPED_ATTRIBUTE_WAS_DIRECTORY or something, may or may not be enough

    SLOP t = open_from_drive_path(fsp, object_access_kind);

    return t.literal_memory_mapped_from_tracked();
  }

  if(s.is_tracking_memory_mapped())
  {
    // POP lazy load child pointers that are MEMORY_MAPPEDs instead of actively loading them at open time, track this using MEMORY_MAPPED_ATTRIBUTE_RLINK_EXPANDED_INITIALIZED, and you can maybe postpone it until the first "good slop" for the TRANSIENT slab is requested. Feature. toggle to actively warm this at load time
    SLOP t = s.literal_memory_mapped_from_tracked();
    A_MEMORY_MAPPED &e = *(A_MEMORY_MAPPED*)t.presented();
    e.cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_RLINK_EXPANDED_INITIALIZED, (I)true);
    bool expanded = (bool)e.get_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_RLINK_EXPANDED_INITIALIZED);
  }

  // Feature: check replacement_count for validity, in particular CHUNK_TYPE_RLINK3/RLINK3_ARRAY should have count() == replacement_count

  // Feature this is presumably where `_parted` reads would take place, joining them to the existing table in `s`

  return s;
}

SLOP FILE_OPERATIONS::open_from_drive_path_multifile_expanded(std::string path, DRIVE_ACCESS_KIND access_kind)
{
  // NB. See FILE_OPERATIONS::write_to_drive_path_multifile_expanded for the spec and FILE_OPERATIONS::open_from_drive_path_directory_expanded for the implementation to copy

  // TODO implement, for both read and mmap versions, look for sibling files - skip for now
  // TODO further, the multifile read/mmap versions are so similar they likely factor into open_from_* versions, descending from here

  return open_from_drive_path_flat_singlefile(path, access_kind);
}

SLOP FILE_OPERATIONS::open_from_drive_path_flat_singlefile(std::string path, DRIVE_ACCESS_KIND access_kind)
{
  switch(access_kind)
  {
    case DRIVE_ACCESS_KIND_READ_WORKSPACE:
    case DRIVE_ACCESS_KIND_POSIX_READ_FLUSH:
      return read_from_drive_path_flat_singlefile(path);
    case DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE:
    case DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY:
    case DRIVE_ACCESS_KIND_MMAP_PRIVATE_WRITEABLE:
      return memory_mapped_from_drive_path_flat_singlefile(path, access_kind);
    default:
      die(Undefined drive access kind for open_from_drive_path_flat_singlefile)
      return NIL_UNIT;
  }
}

#pragma mark - 

SLOP FILE_OPERATIONS::read_from_drive_path(std::string path)
{
  return open_from_drive_path(path, DRIVE_ACCESS_KIND_POSIX_READ_FLUSH);
}

SLOP FILE_OPERATIONS::read_from_drive_path_directory_expanded(std::string path)
{
  return open_from_drive_path_directory_expanded(path, DRIVE_ACCESS_KIND_POSIX_READ_FLUSH);
}

SLOP FILE_OPERATIONS::read_from_drive_path_multifile_expanded(std::string path)
{
  return open_from_drive_path_multifile_expanded(path, DRIVE_ACCESS_KIND_POSIX_READ_FLUSH);
}

SLOP FILE_OPERATIONS::read_from_drive_path_flat_singlefile(std::string path)
{
  const bool SUCCEED = false;
  const bool FAIL = !SUCCEED;

  SLOP x = NIL_UNIT;

  auto lambda = [&] () noexcept {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if(!in)
    {
      std::cerr << std::strerror(errno) << '\n';
      return FAIL;
      // Remark. It's actually OK to ERROR inside here, but bool returns are simpler to reason about
    }
    const auto length = std::filesystem::file_size(path);

    // Feature. other sanity checks
    // Feature./POP. I suppose it's possible that filelength exceeds required length (eg, 128 byte file contains 64 byte charvec per charvec->n value). We wouldn't intentionally write that, but...
    if(length < MINIMAL_ALIGN) // this isn't right, exactly? minimal size is minimal header? coincidence only? I mean, we'll probably require the smaller 8byte (slabs are 16byte) version split it in half 4==4 (|header|==|payload|), and any 4 byte split it in half 2==2, and so on, meaning this would hold anyways.
    {
      std::cerr << "File size too small to be valid: " << (length) << "\n";
      return FAIL;
    }

    // Feature. Technically, this is critical section, and we should:
    // 1. critical section (wrapper):
    //    1A. mock up the first 8 bytes of the slab as a charvec (n := filelength)
    //    1B. register the charvec slab on the SLOP so it isn't lost to ctrl-c
    // 2. read the first 8 bytes of the file into a temp buffer
    // 3. read the next n-8 bytes into the slab, offset 8
    // 4. memcpy the 8 bytes of temp buffer over the slab header
    // 5. touch up and/or verify the intransmissible values (eg, sutex, ...)

    MEMORY_POOL &my_pool = SLOP::get_my_memory_pool();
    SLAB* s = (SLAB*)my_pool.pool_alloc_struct(length);

    in.read((char*)s, length);

    {
      // HACK: technically this is all dependent on t_slab_object_layout_type
      // zero_bus_intransmissible_values: sutex, arena, perhaps m
      // ¬PROT_WRITE will merely verify these
      s->reference_management_arena = REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK;
      s->m_memory_expansion_size = floor_log_2(length);
      s->r_slab_reference_count = 0;
      s->sutex = {0};
    }

    bool memory_mapped_descend = false;
    x = SLOP(s, 0, false, true, memory_mapped_descend);

    // TODO. I think you don't know ahead of time whether this file has gaps in its RLINKS/SLAB4s (dir/multifile), so you have to check it after load? and maybe populate those elts?
    // TODO. ^^ this is skew to reading in a flat singlefile and wanting to unflatten it, which we should probably just do anyway: Alternatively. we do this at alter time, when we're altering a LAYOUT LIST or something, we unflatten just enough recursively up to the top, eg from the column we want to append to up through to the root table?

    return SUCCEED;
  };

  auto result = file_rw_safe_wrapper(lambda, path, DRIVE_ACCESS_KIND_POSIX_READ_FLUSH);

  if(result)
  {
    ERROR(ERROR_FILE);
  }

  // Warning. You can't log x to the console here/below because it may have nullptr references until populated above

  return x;
}

#pragma mark -

SLOP FILE_OPERATIONS::memory_mapped_from_drive_path(std::string path, DRIVE_ACCESS_KIND map_kind)
{
  return open_from_drive_path(path, map_kind);
}

SLOP FILE_OPERATIONS::memory_mapped_from_drive_path_directory_expanded(std::string path, DRIVE_ACCESS_KIND map_kind)
{
  return open_from_drive_path_directory_expanded(path, map_kind);
}

SLOP FILE_OPERATIONS::memory_mapped_from_drive_path_multifile_expanded(std::string path, DRIVE_ACCESS_KIND map_kind)
{
  return open_from_drive_path_multifile_expanded(path, map_kind);
}

SLOP FILE_OPERATIONS::memory_mapped_from_drive_path_flat_singlefile(std::string path, DRIVE_ACCESS_KIND map_kind, bool literal_memory_mapped, I offset, bool headerless)
{
  // Remark. For the most part we just want to mmap in the entire file and then produce a temporary pointer that's been offset to the object we want (typically, the offset will be 0, but suppose we want to mmap in just one column of a large flat singlefile table). You could attempt to mmap in only the portion you're using, but this presents several difficulties: your object may not live on the imposed page boundary, it may not properly end on one, these differences will produce a width that deviates from the filesize, and it is currently unclear what implementing this additional logic buys us. Further, if you attempt to do this, you could claim that only that region is locked, and this implies tracking subregions in the FILE_REGISTRY. Again, for what purpose? (Later, perhaps, this could maybe be used as part of a kerf1 style edit-map-shared-files-in-place kind of thing.) For now I believe this is best avoided.

  SLOP m(PREFERRED_MIXED_TYPE);
  m.coerce_to_copy_in_ram(false);
  SLOP n = MAP_UPG_UPG;
  n.amend_one((I)A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_PATH, ""); // sic, before changing presented type
  m.layout()->cow_append(n);
  m.cowed_rewrite_presented_type(MEMORY_MAPPED);

  A_MEMORY_MAPPED &e = *(A_MEMORY_MAPPED*)m.presented();
  //principal
  e.cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_DRIVE_ACCESS_KIND, (I)map_kind);
  e.cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_OFFSET_OF_OBJECT,  (I)offset);
  e.cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_HEADERLESS,        (I)headerless);
  // derived
  e.zero_derived();

  bool failed = false;

  critical_section_wrapper([&]{
    // critical section:
    //   modify the refcount in the registry
    //   put the key on the MEMORY_MAPPED slab (so it unregisters on free)
    // Warning. don't ERROR/jmp inside critical section
    failed = The_File_Registry.register_path_use(path, map_kind);
    if(!failed) e.cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_PATH, path);
  });

  if(failed)
  {
    ERROR(ERROR_FILE);
  }

  // return a SLOP with the actual MEMORY_MAPPED slab with all the metadata, or passthrough to the mmap'd vector(s) on the drive?
  if(literal_memory_mapped) return m;

  SLAB *a = m.slabp;
  return SLOP(a);
}

#pragma mark - Workspace

void FILE_OPERATIONS::workspace_save(std::string path)
{
  if("" == path) path = workspace_default_path();

  The_Kerf_Tree_Parent_Sutex.sutex_safe_wrapper_shared([&]{
    SLOP w(The_Kerf_Tree);
    write_to_drive_path(w, path, DRIVE_ACCESS_KIND_WRITE_WORKSPACE);
  });
}

void FILE_OPERATIONS::workspace_load(std::string path)
{
  if("" == path) path = workspace_default_path();

  SLOP w{};
  w = open_from_drive_path(path, DRIVE_ACCESS_KIND_READ_WORKSPACE);

  The_Kerf_Tree_Parent_Sutex.sutex_safe_wrapper_exclusive([&]{
    critical_section_wrapper([&]{
      kerf_tree_deinit();
      w.coerce_parented();
      w.legit_reference_increment();
      The_Kerf_Tree = w.slabp;
    });
  });

  return;
}

std::string FILE_OPERATIONS::workspace_default_path()
{
  return "workspace/";
}

#pragma mark -

} // namespace
