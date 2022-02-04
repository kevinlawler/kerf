#pragma once
namespace KERF_NAMESPACE {

struct TRANSMIT_NODE;
struct TRANSMITTER;

struct EMITTER
{ 
  bool bytes_remain_to_read = true;
  TRANSMIT_NODE *parent;
  virtual I read_1_byte_write_to_buffer(char *buffer){return read_up_to_n_bytes_write_to_buffer(buffer, 1);};
  virtual I read_up_to_n_bytes_write_to_buffer(char* buffer, I n){I bytes_read; return bytes_read;};
  virtual I brute_write_1_byte_read_from_buffer(char* buffer){return brute_write_n_bytes_read_from_buffer(buffer, 1);};
  virtual I brute_write_n_bytes_read_from_buffer(char* buffer, I n){I error_code; return error_code;};
};
struct EMITTER_DESCRIPTOR_HANDLE: EMITTER { };
struct EMITTER_SOCKET_DESCRIPTOR_HANDLE : EMITTER_DESCRIPTOR_HANDLE { };
struct EMITTER_PIPE_DESCRIPTOR_HANDLE : EMITTER_DESCRIPTOR_HANDLE { };
struct EMITTER_FILE_DESCRIPTOR_HANDLE : EMITTER_DESCRIPTOR_HANDLE { }; // aka IS_REG regular file

struct EMITTER_FILEPATH : EMITTER_FILE_DESCRIPTOR_HANDLE // Question. regular files? symlinks? directories?
{
  EMITTER_FILEPATH(std::string s);
};

struct EMITTER_FILE_STAR_STREAM : EMITTER_FILE_DESCRIPTOR_HANDLE
{
  // Refresher: POSIX calls `FILE*->drive` a STREAM and a HANDLE. POSIX calls `int->drive` (or `int->pipe` or `int->socket`) a DESCRIPTOR (`fd`/`filedes`) and HANDLE. Both refer to `FILE DESCRIPTIONS` (note: `descript-*ions*` not `-*tors*`) which just means the file/pipe/socket/&c itself. POSIX distinguishes pipes/sockets/&c from so-called "regular" files (IS_REG), say on the drive. See our use of `fstat`.

  // optimistically, maybe we can convert this to a file handle. In Kerf1 we just used fwrite. Pr. we had checked and not found conversion. - seemingly `fileno` will do this? (and in fact we used this `fileno` trick in kerf1. we didn't everywhere b/c we already had buffer functionality that worked with `FILE*`. ) The reverse direction is `fdopen`.
};

struct EMITTER_POINTER_POINTER : EMITTER
{
  EMITTER_POINTER_POINTER(void *p);
  EMITTER_POINTER_POINTER(SLAB* s);
};

struct EMITTER_NULL_POINTER_POINTER : EMITTER_POINTER_POINTER
{
  EMITTER_NULL_POINTER_POINTER(void** p);
};

struct TRANSMIT_NODE // point of this is to avoid n^2 constructors
{
  TRANSMITTER *parent;
  EMITTER *emitter = nullptr;

  TRANSMIT_NODE(std::string s) { emitter = new EMITTER_FILEPATH(s); }
  TRANSMIT_NODE(void**p) {if(p==nullptr) { /* TODO error */ ;}  emitter = (*p == nullptr) ? new EMITTER_NULL_POINTER_POINTER(p) : new EMITTER_POINTER_POINTER(p); }
  TRANSMIT_NODE(FILE*f);
  TRANSMIT_NODE(TRANSMITTER t); // chaining?
  TRANSMIT_NODE(int h)
  {
    struct stat c;
    if(0 > fstat(h,&c))
    {
      perror("File handle status failed");
      // goto failed;
    }

    bool regular_file  = S_ISREG( c.st_mode);
    bool link_handle   = S_ISLNK( c.st_mode);
    bool is_dir        = S_ISDIR( c.st_mode);
    bool socket_handle = S_ISSOCK(c.st_mode);
    bool pipe_fifo     = S_ISFIFO(c.st_mode);
    bool char_term     = S_ISCHR( c.st_mode);
  }
  // TRANSMIT_NODE(SLAB **s) : TRANSMIT_NODE((void**)s){};
  // TRANSMIT_NODE(SLOP &&s); // &s can write but &&s only read. We should be careful with SLOP because it manages its own SLAB* with references and such [so we can't just do SLAB** overload and call it a day: we need to detect when the SLAB* changes and call the corresponding SLOP methods]. So we can do this, but we need to wrap our SLOP-emitter object. A simple way is to do known_flat in memory.

  bool writeable_emitter(); // SLOP&& can't past a certain point

  I read_1_byte_write_to_buffer(char* buffer){return emitter->read_1_byte_write_to_buffer(buffer);}
  I read_up_to_n_bytes_write_to_buffer(char* buffer, I n){return emitter->read_up_to_n_bytes_write_to_buffer(buffer,n);}
  I brute_write_1_byte_read_from_buffer(char* buffer){return emitter->brute_write_1_byte_read_from_buffer(buffer);}
  I brute_write_n_bytes_read_from_buffer(char* buffer, I n){return emitter->brute_write_n_bytes_read_from_buffer(buffer,n);}

  TRANSMIT_NODE();

  ~TRANSMIT_NODE()
  {
    if(emitter) delete emitter;
  }

  bool had_error();

};

struct TRANSMITTER
{
  // TODO checklist item: make sure we set the proper arena on the receiver side. all subelements should be a transient-like arena (0), which can be preserved on both sides of the wire. 
  // TODO zero the sutex rwlock on any receiving side

  // Remember. Whatever you do in memory is going to pale in comparison to sending it over the wire, so just zip it, filter it, do whatever.

  // Remark. Just write some bad ones with the intent of going back and refactoring. to get a foothold

  // Idea. the thing we want on lhs is maybe `->ready(char buf[], I length)`? then on the rhs? in the middle? how do we call this without imposing need for buffer?
  //       maybe: middle->give_me(MAX, offset) left->i_have(revised) middle->do_this_left middle->consume_right[i]

  // Idea. We can do a percentage-tracker if we want. Ask the LHS for two numbers: known total size of existing arg, and known total size of the produced, filtered output from the existing arg. Return say "-1" for sizes it doesn't know or can't compute easily. Then, as bytes are read/written, update the counter for the in-progress size relative to the total. This is interesting I guess for large file transfers, writes, and what not, especially if we have control over the terminal and can make it clean.

  // TODO: TRANSMITTER needs the write lock if it wants to cache filesizes?
  // Remark. The way to proceed is to actually write the simple version with all the restrictions, then relax restrictions one by one and rewrite, incorporating the new functionality.
  // CURRENT_RESTRICTION: Don't write DIRECTORIES or MULTI-FILE yet
  // CURRENT_RESTRICTION: Don't do FANOUT (array of rhs)
  // CURRENT_RESTRICTION: Don't do multi-threading (reader+writers pattern)
  // CURRENT_RESTRICTION: Don't do chaining (>=3 nodes/filters) maybe.
  // CURRENT_RESTRICTION: Don't STREAM at first, do all work in BATCH
  // CURRENT_RESTRICTION: Don't do COMPRESSSION yet
  // POP: remove restrictions

  // POP Linux has tee,splice,vmsplice
  // POP when we multithread, have two buffers, consumers get sutex read lock on buffer pointer, producer fills other buffer while they're reading, uses write lock two swap buffer addresses. Of interest: `readv` `writev`

  // TODO sending kerf tree over network, need to reassemble locks on the other side. You can either transmit the RW_LOCKS and rewrite the mutex on the other side, or you can not transmit them and then detect the map_is_kerf_tree_map bit on the other side and re-add RW_LOCKs to the nodes of the tree with the bit set. Remember. If you're using std::shared_mutex, then you're going to need to do sizeof() allocs on the other side instead of knowing in advance, probably.

  // Remark: I dunno how useful fanout is (potentially for some kind of weird backup scenario - rare. or some other strange scenario involving multiple formats. seems thin overall). You know, for writing to multiple net sockets (d-kerf), FANOUT WOULD be useful, EXCEPT we can easily multithread that at a higher level. not if the thing doesn't fit into memory tho... It's also highly useful if we're re-broadcasting to several machines on a network (feed handler).

  // Remark. FILTERS may be an array on this thing.

  // Idea. Corner-cutting: Should we do everything in in-memory-batch (ie, build a COMPACTED vector) before flushing to disk [re: rhs]? (Avoiding buffered streaming.) Similarly, zipping every object fully-at-once instead of streaming?
  // Idea. Would be nice if we could pass "(start_position, length)" read args to the lhs from everywhere. 

  // Idea. we can populate operator<< and operator>> in SLOP to use a default transmitter transparently for serialization

  TRANSMIT_NODE reader;
  std::vector<TRANSMIT_NODE> writers;

  I buffer_position = 0;
  I buffer_size = PAGE_SIZE_BYTES;
  char* buffer;

  bool finished = false;
  bool has_error = false;

  // PUTATIVE ////////////////
  I size_of_lhs = -1;
  I transmitter_indicated_wanted_size_of_rhs = -1;
  I available_capacity_of_provided_rhs = -1;
  std::map<void *, I>object_size_cache; // P_O_P: only caches sizes > some minimum
  ////////////////////////////

private: // NB. these bool are private until we implement them [functionality implied by their other truth halves].
  // Idea. Separate these attributes into {drive, inet, both, neither}, or make a table of T/F for {drive, inet, ram} etc
  // Pointer-only args //////////////////////
  bool transmittingFlatContiguousVersusAllocatingInMemoryChildrenOrMultipleFiles = true;
  bool canMoveOrMremapPointerArg = true; // TODO must callback to emitter so SLOP can release_slab_and_replace_with(.), etc.
  bool shallowVersusDeepCopy = false; // note: can only do this for in-memory copies, not inet or drive ?
  bool incrementChildPointers = true;
  bool referenceIncrementOnDetectedIdentityOperationInsteadOfForceCopy = false;
  // TODO: pointer has associated fixed capacity c? perhaps we will or will not do this
  ///////////////////////////////////////////
  // Streaming //////////////////////////////
  bool STREAMINGdecompressIncomingCompressedStreamDataAsOverSocket = false;
  bool STREAMINGcompressOutgoingUncompressedStreamDataAsOverSocket = false;
  ///////////////////////////////////////////
  bool writingSingleFileInsteadOfMultipleFilesOrDirectory = true;
  bool writeMmappableAppendableAmendableVERSUSWriteCOMPACTIFIED = false;
  bool literalCopyEgTapeHeadInsteadOfRepresentational = false;
  bool doNotMultithreadEgPtrToPtrCopies = true; // Remark. Some stuff we don't want to be threaded, like ptr to ptr writes (copies).
  bool followsISLNKSymlinksInsteadOfOverwritesSymlinks = false;
  bool isPurePassthroughWithoutIntermediateBuffer = false; // Remark. stream functions (modifying input) versus pure passthrough (writing pipe to pipe, opt without buffer)
  int identityOrHTONOrNTOH = 0; // Idea. Don't handle Endianness Big-Endian Little-Endian here [batch version], even in the STREAMING version really. Simply set little/big endian HOST on the sender's MESSAGE header thing, then the receiver can fix things up on the other side. (If the hosts match there's nothing to do.) In-memory this is fast and pales in comparison to network transfer time. On-disk, it already involves a disk swap. This is performant and good. Best of all it's modular so you can postpone it.  Note: Use network-order in MESSAGE.
  int readerIsBinaryFormatOrJSONFormatOrDisplayFormat = 0;
  int writerIsBinaryFormatOrJSONFormatOrDisplayFormat = 0;
  bool prependsMessageHeader_MaybeMaybe = false;
  bool consumesMessageHeader_MaybeMaybe = false;
  bool writeSlabHeaderOrNot = true;
  bool readsFromDriveToRAMAsMMAPPED_OBJECT = false;
  int writes_unchanged_or_force_regular_OR_ZipArray_OR_ZipList_OR_LIST_OR_JUMPLIST_OR_LISTZIPLISTLIST_OR_LISTZIPARRAYJUMPLIST = 0;
  bool rehydrateUncompactIncomingDataIfNotAlready = false; // We have a model in mind where arrays are "always" written to disk transparently compressed (say via lz4) but when you read them back you need to decompress them transparently as well.
  bool dehydrateCompactOutgoingDataIfNotAlready = false;
  bool zeroesPointersOnWrite = false; // for writing non-flat elements, zero RLINK3 pointers and SLAB4's embedded pointers before they hit the drive/network
  int writesLiteralMemoryMappedNotRepToDrive = false;
  int transmitsLiteralMemoryMappedNotRepOverInet = true;
  int InetRecipientShouldRequestCorrespondingFilesForMemoryMappedReferencedPath = true;
  bool unflattensEgMakesAllAppendableAnyReceivedSinglefileFlatObjectFromNetworkOrDrive = true;

public:

  TRANSMITTER(TRANSMIT_NODE lhs, TRANSMIT_NODE rhs)
  {
    reader = std::move(lhs);
    writers = {std::move(rhs)};
  }

  void finish()
  {
    finished = true;
  };

  void start()
  {
    //assert reader can read
    //assert writer can write
    while(pump());
    finish();
  };

  bool pump()
  {

    // this PULLS from lhs. but we probably want lhs to PUSH into multiple buffers (call it FEED or FLOW). after each buffer is filled (or lhs can't read/write anymore) then have writers move to the next buffer.
    // Remark. Bug. Our model is probably broken. Instead of TRANSMITTER having a left and right, Nodes should have an array of nodes to the right, and then a transmitter maintains one root node. Then you can chain. Maybe none of this is necessary if we don't have a use for it. It's CERTAINLY not useful in BATCH mode instead of streaming.

    // POP sometimes the buffer can just be the right-hand side's pointer instead of our intermediate TRANSMITTER.buffer. Some of those may be determined by TRANSMITTER(specific_class1 x, specific_class2 b)

    I available = buffer_size - buffer_position;
    char* relative_buffer = buffer + buffer_position;
    int bytes_read = 0;
    bool still_reading = reader.emitter->bytes_remain_to_read;

    if(still_reading) 
    {
      int bytes_read = reader.read_up_to_n_bytes_write_to_buffer(relative_buffer, available);
      if(bytes_read < 0 ){} // error or stream closed
      still_reading = reader.emitter->bytes_remain_to_read;
    }

    if(bytes_read > 0)
    {
      for(auto writer : writers) // POP this is where you would parallelize
      {
        int error_code = writer.brute_write_n_bytes_read_from_buffer(relative_buffer, bytes_read);
        if(error_code){}
      }

      buffer_position += bytes_read;
      if(buffer_position >= buffer_size) buffer_position = 0;
    }

    bool still_pumping = still_reading;

    return still_pumping;
  }

  bool had_error = false;
  SLAB* resultant_pointer = nullptr;

  TRANSMITTER()
  {
    buffer = new char[buffer_size];
  }

  ~TRANSMITTER()
  {
    delete[] buffer;
  }
};


} // namespace
