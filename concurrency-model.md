# Kerf2 Concurrency Object Model: Fully Multithreaded, Thread-Safe, Signal-Safe, Mmappable-Safe, No Leaks, Converging to Optimal Performance

We need a thread-safe object model that works in the context of Kerf Userspace and in the context of the Kerf Internals. Primarily this will be atomic reference counting with some use of locks and a central Kerf Tree. The reference counting will largely work as in Kerf1 or in Kona or other arraylangs. The model has the added constraint that execution should be able to pause at any time and unwind without memory leaks or other disturbances. Further, we should be able to concurrently access via mmap an arbitrarily large number of files on the disk in excess of open file handle limits, reading, writing, creating, updating, and appending objects to and from these files without additional protections. Thread Pools should be initializable and run from anywhere without overhead or concern. All these requirements should be simultaneous and interoperable and capable of being used in combination including inside new internal object types and in userspace without the overhead of parallelization management. We ask in spite of these constraints that execution proceed in a near-optimal way.

I should able to spawn hundreds of parallel readers and writers, who are each able to mmap complex nested object hierarchies, who have their own nested thread pool for computation, who pass data between all of these threads; within a line or two, without worrying about what a lock or a pthread is, and then interrupt its execution and resume what I was doing before without trashing the workspace. Then I can start it over or nest it recursively. Internally, we should be able to compose the scope of such userspace actions into new and useful objects, again recursively.

Until we achieve at the process level what was previously achieved by the server we cannot achieve at the server level what was previously achieved by the process.

## Arenas & Thread-safety

Data is divided into four different arenas: `UNMANAGED_TRANSIENT,` which is basically automatic or stack data (eg, an automatic C array that lives only inside the brackets of a particular C function, alloca data, and so on); `CPP_WORKSTACK` data, which designates thread-local-ish data tracked on a special workstack (eg, a heap/pool allocated object created for use inside a C++ function); `KERFVM_WORKSTACK,` which is temporary-ish data that is pushed on a KerfVM (eg, the previous but marked for death on KerfVM); and `TREE_OR_PARENTED` data, which is data that may be referenced elsewhere, including by other objects or other threads or by the Kerf Tree, and which represents most of the multithreading concerns (eg, a subobject pointed to by a list of pointers, or an object at one point visible globally on the Kerf Tree).

Data objects in Kerf are called SLABs. The arena that the SLAB lives in determines its threading rules. Each SLAB stores its arena using two bits.

Transient data we don't care about and we don't try to take ownership of it or point to it in a lasting way, i.e., a way that outlives the stack (we'd have to copy it first thereby promoting it). `CPP_WORKSTACK` is managed predictably by the workstack. Typically this data is freed at the end of the function in the referencing C++ object's destructor. `KERFVM_WORKSTACK` is similar, except we understand that we can repurpose it inside the KerfVM if it will pop anyway. Workstack data that becomes a child or hits the Kerf Tree or needs to achieve better thread-safety guarantees is promoted to `TREE_OR_PARENTED.` `TREE_OR_PARENTED` is the only one that really needs atomic counters.

Generally speaking, internal Kerf Threads don't see each other's automatic or workstack data, only each other's `TREE_OR_PARENTED` data. Internally we have the option to violate these rules when we know what we're doing. 

## Signal and Error Safety

`Ctrl-c` (`SIGINT)` can happen at any time. We catch it, wind down execution, and recovery totally from it. No memory is leaked. We recover from locks. These means every line of code must leave a consistent state or shield itself from interruption. See <https://kevinlawler.com/ctrl-c> for more discussion.

Same for Errors. 

## C11/C++11

Since C11/C++11 atomicity is enforced by the `__atomic` family.

## Normalized Thread-Ids

Instead of using randomized `pthread_id`'s we normalize thread ids to be 0, 1, 2, ... up to a thread max. This enables a few improvements. In particular, it lets us easily identify who owns an exclusive lock.

## Reference Counting

Reference counting works in the typical way. An object begins life as unitialized data. Then it becomes a SLAB with reference count 1, representing the owning thread or the object's parent. The reference count may fluctuate above 1 to 2^{`r_slab_reference_count`}. When the reference count hits zero the object is freed. A count of zero signals that nobody else is looking at the object and so it's safe to be freed.

Transient data isn't reference counted. Workstack data is reference counted non-atomically for use by the producing thread and typically for the life of the called C++ function in the stack. `TREE_OR_PARENTED` data is reference counted atomically and generally refers to global data, even if it sometimes just means children of other objects within a single thread.

Workstack objects are tracked by a workstack. So if we capture a signal we can begin freeing them. Instead of the Kerf Tree, `CPP_WORKSTACK` objects are tracked by this workstack. This is how they don't get lost.

`TREE_OR_PARENTED` objects are tracked either directly by the Kerf Tree or by some ancestor that's recorded in a workstack. This is how they don't get lost. If a signal interrupts us, `TREE_OR_PARENTED's` the relevant ancestor either lives on the Kerf Tree, so everything sticks around, or the ancestor lives in a workstack, and so everything is freed, or rather, the ancestor is reference decremented and recursively reference decrements its children only if it's freed.

Reference counting follows copy-on-write semantics. If a thread has a reference to a SLAB with reference count 1, it is free to write to it, or free it and replace it with another object, modifying the pointer to it in any potential parent as necessary (provided all rules are observed regarding the reference counts in the parent and the ancestral chain). If the reference count is two or more then first the data is copied into a new SLAB and then the reference count is decremented.

A SLAB with reference count 1 either lives on the Kerf Tree and you can write it because you have the lock, or it lives only in your thread and you can write it because nobody else knows about until you initiate a handoff. (This assumes any ancestry of the item is also all reference count 1 and ready to allow writes in its descendants.)

`CPP_WORKSTACK` SLABs are incremented by other C++ automatic objects, typically. `TREE_OR_PARENTED` objects are incremented by references from other parent SLABs. Notably, their reference count is not increased by references from C++ automatic objects. This lets us interact with them, including writes and reads, using the same referencing objects as for `CPP_WORKSTACK.`

In the case of `TREE_OR_PARENTED,` these reference increments and decrements need to be atomic because another thread might have acquired it via the Kerf Tree (or some other method). Without atomic decrements this might result in two threads decrementing the counts at the same time.

Consider Threads A and B. Each acquires a child C for internal processing from .d on the Tree. The refcount of C is 3. Then the Kerf Tree is wiped, the refcount of C becomes 2, and both A and B complete their processing. If they both see 0, it's a double free. If they both see 1, it's a memory leak. We need atomicity to ensure 1 and 0 are both seen and in order.

## Kerf Tree Concurrency

Visitations to the Tree are protected by a Kerf signed mutex called a Sutex which ensures synchronicity. A sutex allows multiple readers or one writer to control the lock.

The Kerf Tree is acyclic, and this must be controlled during assignment, as it must be during other `TREE_OR_PARENTED` assignments, whether implicitly by guaranteeing the impossibility or explicitly by cycle-checking. Usually cycle-checking is fast: vectors of integers do not contain pointers, and objects that do are usually neither deep nor long.

### Reading & Writing: Locking the Kerf Tree

Owning a read lock permits readers to inspect the data and as necessary increment the reference counter (which, technically, need not be atomic if it would be published by relinquishing the lock). If the reference counter is incremented, the thread can relinquish the lock and continue to read the data, with the added obligation of ensuring the reference is decremented later.

Controlling the write lock allows threads to modify and replace data on the Kerf Tree, subject to copy-on-write considerations.

Locks typically must live at a level up from the data in question so that they aren't freed with the data. This includes the root node.

There are two different ways we could perform locking on the Kerf Tree. I haven't tested which one is better but I suspect it's Option 2. It may actually depend on the usage model. Basically, the tradeoff is between maintaining paths through the tree versus not maintaining paths but claiming subtrees. If the paths are long or contentended it may make sense to use subtrees. If the subtrees are big it may be better to use paths.

Let the root of the inverted tree be R, A its first descendant, B the grandchild, C the great-grandchild. Let S be the data at C. Nodes A, B, C, and S may have unspecified siblings not locked. The subtree at C does include all siblings below C. "Reading" S may involve assigning it as a child somewhere else, potentially off the tree, in which case a reference decrement would not be necessary, and future reads can generally take place unprotected by locks. 

Reference count > 1 in a non-leaf node is an unusual event, but happens if you attach a nested MAP to the tree or copy a library or some such.

Attempting to read intermediate nodes that do not exist throws and undefined token error. Attempting to write intermediate nodes that do not exist creates them.

When assigning MAPs to the tree, we must recursively update any MAPs to use a format which will not upgrade its head SLAB, like `MAP_YES_YES` (I don't remember the minimal such). This simplifies operations. The key vectors and value vectors can all be reused without modification.

Sutexes and compiled references can live wherever, either in an associated attribute MAP or as bits somewhere on the main SLAB.

#### Option 1: Ancestor Path Locking

To read S, the thread shared-locks R,A,B,C, atomically reference increments S, unlocks C,B,A,R, then reads S, then atomically reference decrements S.

To write S, the thread path locks shared as before except it exclusively locks C. Here we don't need C's subtree because the absence of shared paths intersecting C guarantees the absence of locks in the children. Reference incrementing S is not necessary. 

For writes we also need to COW all tree nodes (and S) with reference count > 1 along the way, and this requires stopping to acquire exclusive locks in ancestors. We can do this by upgrading locks in the path, COWing, and downgrading locks and proceeding.

#### Option 2: Crabbing + Subtree Locking

To read S, the thread shared-locks R, shared-locks A, unlocks R, shared-locks B, unlocks A, shared-locks C, unlocks B. The thread shared-locks the remaining subtree at C. Then it atomically reference increments S, unlocks the subtree including C, reads S, then atomically reference decrements S.

If S is a leaf, there is no subtree. Without read locking the subtree we cannot be sure the subtree is free of writers. Writers assumed the path had refcount 1. Violating the refcount guarantee would mean the reader tries to read from an object with an indeterminate state.

To write S, the same process happens except we exclusive-lock the entire subtree at C. This is necessary in order to flush pending locks through the leaves. Reference incrementing S is not necessary.

For writes we also need to COW all tree nodes (and S) with reference count > 1 along the path, and this involves locking subtrees. As a consequence, we may exclusive lock higher and massage everything in the same lock.

When locking subtrees for writing, the purpose is to flush remaining writers, so it isn't strictly necessary to acquire the locks as long as you watch for other locks to leave or can ensure somehow they've left.

### References From Functions to Globals

User functions or lambdas are assembled as bytecode and reference global variables stored on the Kerf Tree. There are two ways to do this.

#### Option 1: Symbolic References (Slow, Flexible)

The symbol ".A.B.C.D" is stored on the function and every read/write to ".A.B.C.D" involves a symbolic traversal of the tree, Root to A to B to C to D, as described in the locking algorithms above.

#### Option 2: Compiled References (Fast, Inflexible)

Compiled references permit direct access of the location without traversing the tree. The lambda stores a tuple that contains a pointer to the tree node and an integer value representing the position of relevant [key/]value payload. Assuming we build our tree using the current Kerf MAPs as nodes, we cannot store a pointer to the value payload pointer directly because its memory location may change when we reallocate the value list to incorporate new values at the end. The MAP node is more resilient to change and so maybe we can point to it. We still need to ensure the address of the node does not change.

The way we ensure nodes remain at the same address is through a new category of reference count called a compiled reference count.

To create a compiled_reference to ".A.B.C.D", we traverse Root to A to B to C to D in order using one of the methods above. We need write locks. At each place we copy-on-write the node as needed in order to ensure regular reference count 1. If it isn't already guaranteed by tree assignment, at this point we also need to make sure the MAP used at the node is of a kind who will not change its head SLAB location by upgrading, say `MAP_YES_YES` (I don't remember the minmal map that guarantees this), since lighter weight maps may have been assigned to the tree. Then we increment the compiled reference count, say to 1. The compiled reference count signals that the regular reference count can not incremented: doing so would break the 1-guarantee in the write path, and so new references must instead copy. Further, nodes which possess a non-zero compiled reference count cannot be removed (overwritten in the tree), as this too would break the chain the function depends on to make updates to the global variable. When the life of the function ends we can decrement all of its compiled references in reverse order.

## Sutexes

A sutex is Kerf's version of a mutex implemented using atomics. It can be compiled any bit length: from 2 on up. These can be packed into any header. At 13 bits you support around 2048 unique threads. The sutex uses a positive count for sharers and an negative count for exclusive writers. The exclusive writer is the one-indexed thread-id.

A sutex is a spin rwlock with recursion/errorcheck [readers/writer] and bzero = safe [re]init/destroy in 2+ bits. It is unfair, unticketed, writer-preferring, and uses exponential PAUSE backoff into sched_yield. It has ERRORCHECK: writers are stored as their normalized id (negative, one-indexed). It is RECURSIVE: reader locks are stored as an atomic count (non-negative). Locks can be upgraded from shared to exclusive and downgraded from exclusive to shared. New sharers block/fail while waiting for a lock to upgrade to exclusive.

Sutex stands for Slab mUTEX or Signed mUTEX or your fav here.

There is a lot of sugar for sutexes. We can track them in stacks and so on.

Sutexes avoid most of the headaches, overhead, and rules of pthread-style mutexes. They can be reset without issue.

## MMAP-Safety

Transparently MMAP-ing files as objects presents many challenges. Data must be considered `UNMANAGED_TRANSIENT` which means you can't write pointers inside of it. Objects may grow and need to be remapped, which changes their locations. On top of that, POSIX systems typically place a low limit on the number of opened file handles, which limits the number of `mmap`'s, which in turn prevents us from mapping the full generality of objects. The solution to this is to use an LRU queue which pages in and out actual `mmap`'s as references from inside of shell objects. Because the LRU queue is external to these `MEMORY_MAPPED` objects this presents a coordination problem in its own right---how do we guarantee data shows up when we go looking, or that data isn't freed from under us while we're reading and writing---and we solve this problem by wrapping accesses to `MAPPED_OBJECTS` inside of sutex locks. 

If we were hoping to apply standard reference counting to `mmap`'ed vectors, that also does not work as expected, because write-enabled `mmap`s are unique: there is no way to copy them sensibly to create a personally writeable copy. Sutex locks solve this as well.

Further complicating things, if we wish to `mmap` a large vector with `MAP_PRIVATE`, and then make a few (private) modifications inside of it, we must restrict `munmap`'ing the data until the object is freed, since the modifications we made will not persist into next `mmap`ing.

## Other

### Kerf Tree: Potential Optimization Points

Though this is not a design goal, single-threaded versions of this array-model architecture using have produced, as a side-effect, several production-tested, performant databases over the years. The single-threaded versions shined when the model was limited to a few hundred or few thousand external connections, the multithreaded versions will expand on that to some degree. This isn't the place to reexplain or argue the effectiveness.

The existing model is probably sufficient for our projected needs. Even so, there is a gradient of performance optimizations that could be applied, including things like per-leaf locking, and fine-grained locking on subsequences of arrays.

You can `unset` variables provided you can acquire the write lock and no compiled references interfere. 

### Overkill: Sutexes On Every Slab

Short version: seems like a mistake, could reclaim the ~13 bits for attributes/etc, minor chance useful later for unforeseen optimizations. We didn't need to put them on every object. I had some other ideas about it which didn't pan.

Sutex mostly useless for this purpose on every object, because sutex will be incorrectly situated where it can be freed, so future obj can't look there to read or write lock, such locks need to persist after life of object

Sutex on object is quite broken. Or rather, quite limited, not even sure what you can do with it, because it can't manage references to the object. The header bits should probably be reclaimed. If you have two thread references and negotiate to modify in place atomically it works (strange), but you can do that non-atomically without a sutex or with one somewhere else. Key point: in place does not mean append.

I guess the benefit is future possible optimizations, eg of tables, you get a write lock on say the head of an `RLINK3` array for expanding columns. But you could just as easily store this sutex in the attributes. I guess I don't understand what purpose sutex-on-every-slab is good for. Maybe it sounded like a good idea but in practice it isn't clear what it does.

### Making Child-Parent-Acquisition Signal-safe 

Question. How do we make reference incrementing signal-safe? Basically every assignment of a `TREE_OR_PARENTED` SLAB as a child to a parent requires a reference increment and then a pointer assignment, both those operations must be done atomically together (see DWCAS, software MCAS, suspected huge perf hit for current gen chips/compilers) or protected by a signal shield for true safety (5 microsecond overhead as of 2021, but you don't want any shield to last longer than human perception. (The point is to avoid a memory-leak when `ctrl-c` happens.) There are faster methods, see jump.cc critical_section_wrapper for creating a "critical section" flag that delays pthread_kill, but this is not worth the effort yet and may never be, to turn micros into nanos in an unimplemented drag on performance everywhere to handle a rare event). Do we have such an atomicity guarantee? Maybe we have to let it slide. I suspect we have a comment referring to this in the source. See eg `SLOP::release_slab_and_replace_with` which indicates we simply ignore.
Ways to solve the signal-safe-reference-incrementing-child-of-parent-dwcas:
Note that you do not get any guarantees on the invariance of the reference count because other threads might be playing with it. Note many of these could be built as wrappers and even stubbed out, so at least we can track where the increments+assignments happen.
1. Ignore it. Solution is fast, rarely leaks memory even on `ctrl-c`. In practice, we're willing to accept a few nines on avoiding memory leaks during ctrl-c versus absolute perfection. 
2. Put everything behind a signal safe shield wrapper. This is 5 microsecond overhead per wrapper, and you can't have a wrapper last longer than human perception (delays `ctrl-c).` My first impression is this is slow and bad.
3. Figure out some kind of software DWCSA/MCAS solution, likely to be slow on current gen chips/compilers in 2026. The memory locations in question aren't contiguous.
4. Refactor our entire codebase and layout to have contiguous memory to make use of current DWCSA. This sounds crazy and bad.
5. There may be some kind of wonky existing DWCAS solution where you create a dummy object parent with contiguous memory and different kerf release rules that references the items in question and then you use a creative DWCAS solution on its contiguous memory, but I didn't figure this out and I'm not sure it works. You'd put it on the thread-local KerfVM or Tree or something. Three-to-four 8-byte entries, you check it special when wiping the stack.
6. Instead of signal safe wrapper, you put an atomic flag inside of the pthread_kill mechanism that says the thread is not ready to die yet. Enable the flag before you do the increment+assignment, then disable. This should cover only a few instructions at a time. If you don't do any human-perceptible delays, this should be one of the best solutions. Performant per increment+assignment and adds only a few nanoseconds. See remarks jump.cc critical_section_wrapper
7. Track the single potentially leaked memory address. You put a single slot for a pointer on the per-thread KerfVM, associated with the workstack. Before you enter the increment+assignment, you atomically relaxed assign the single slot to be the address of the object, after you leave, you atomically relaxed clear it. When `ctrl-c` happens, before all the workstacks are cleared (so you don't touch freed memory???), if the single slot is set (it typically won't be), you walk the Kerf Tree (and any other global storage that might have a reference) and the workstack and infer what the real count of the object is supposed to be by recursively looking for references. (If this reconciliation is happening in an outside thread, you need the memory to be synchronized and not torn. Do we ensure this currently in the cleanup, assuming we use an outside thread?) Then, if the increment happened but not the assignment, you decrement the reference counter (maybe even cleaning the unassigned slot in parent - do we need this? parent should clean its own slot first, perhaps). Otherwise, you can do nothing. More performant than 6 (atomic-flags-for-pkill) on a regular basis, except when `ctrl-c` happens it rarely requires a linear check of the Kerf Tree. Or rather, at most one check per thread, but you could bundle the checks (the same linear pass checks each pointer against all of the active slot and maintains corresponding counts).

