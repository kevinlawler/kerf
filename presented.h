namespace KERF_NAMESPACE {
// Remark. Accessor _3 instead of _Base because of fewer overrides later. We have to use single [virtual] inheritance everywhere or we expand the size of the PRESENTED mechanism in slop past 1*sizeof(V)

struct PRESENTED_BASE : ACCESSOR_MECHANISM_3
{

public:
  /////////////////////////////////////////////////////////////////////////
  SLOP* parent();
  LAYOUT_BASE* layout();

  auto presented();
  auto begin();
  auto end();
  SLOP operator[](const SLOP& rhs);
  /////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////
  static PRESENTED_TYPE_MEMBER presented_int_array_type_for_width(UC width) {return (PRESENTED_TYPE_MEMBER[]){INT0_ARRAY, INT1_ARRAY, INT2_ARRAY, INT3_ARRAY, UNTYPED_SLAB4_ARRAY}[width];}
  static PRESENTED_TYPE_MEMBER presented_float_array_type_for_width(UC width) {return (PRESENTED_TYPE_MEMBER[]){UNTYPED_ARRAY, FLOAT1_ARRAY, FLOAT2_ARRAY, FLOAT3_ARRAY, UNTYPED_SLAB4_ARRAY}[width];}
  static std::string string_for_char(C c, C quote_char = '\\', bool less = false);
  /////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////
  virtual bool is_unit() { return false;}
  virtual bool is_fixed_width_unit() {return is_unit();}
  virtual bool is_grouped() {return false;} // layouts grouped
  virtual bool is_simply_layout() final {return !is_grouped();}
  virtual std::string to_string(){return "base";}
  virtual bool is_nil() const {return false;}
  virtual bool is_error() const {return false;}
  virtual bool is_map() const {return REP_MAP==get_representational_type();}
  virtual bool is_stringish() const {return false;}

  virtual REPRESENTATIONAL_TYPE get_representational_type() const {return REP_NIL;}

  virtual bool requires_smallcount_unpacked() { return false; }

  virtual I I_cast();
  virtual F F_cast();
  virtual CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA;}

  virtual SLOP presented_index_one(const SLOP &rhs);
  virtual SLOP presented_index_many (const SLOP &rhs);

  virtual void dealloc_pre_subelt() noexcept {};
  virtual void dealloc_post_subelt() noexcept {};

  virtual SLOP last();

  virtual I count() { return layout()->payload_used_chunk_count(); }

  virtual void cow_append(const SLOP &rhs);
  virtual void cow_join(const SLOP &rhs);
  virtual void cow_amend_one(const SLOP& x, const SLOP& y);
  virtual void cow_delete_one(const SLOP& x);
  virtual SLOP cowed_insert_replace(const SLOP& x, const SLOP& y);

  virtual std::weak_ordering compare(const SLOP& x);

  virtual SLOP indices();
  virtual SLOP keys();
  virtual SLOP values();

  virtual SLOP get_attribute_map();
  virtual void cow_set_attribute_map(const SLOP &a){};

  virtual void callback_from_mapped_queue_to_inform_removed() {}
  virtual void callback_from_mapped_queue_to_store_index(I index){}
  virtual bool allow_to_copy() {return true;}
  virtual bool is_secretly_a_memory_mapped() {return false;} // this is obsolete unless you're using the overload-all-MEMORY_MAPPED-methods technique
  /////////////////////////////////////////////////////////////////////////


public:
  virtual HASH_CPP_TYPE hash(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed) final;
protected:
  virtual HASH_CPP_TYPE hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed);
private:
  friend std::ostream& operator<<(std::ostream& o, PRESENTED_BASE& x) {return o << x.to_string();} 
};

//////////////////////////////////////////////////////////////////////////

struct A_UNIT : PRESENTED_BASE
{
  bool is_unit() {return true;}
  std::string to_string() {return "unit";}
  HASH_CPP_TYPE hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed);
};

struct A_NIL_UNIT : A_UNIT
{
  REPRESENTATIONAL_TYPE get_representational_type() {return REP_NIL;}
  bool is_nil() const {return true;}
  std::string to_string() {return "null";}
};

struct A_CHAR3_UNIT : A_UNIT
{
  REPRESENTATIONAL_TYPE get_representational_type() {return REP_CHAR;}
  std::string to_string();
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARINT;}
  bool is_stringish() const {return true;}
};

struct A_INT3_UNIT : A_UNIT
{
  REPRESENTATIONAL_TYPE get_representational_type() {return REP_INT;}
  F F_cast();
  std::string to_string();
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARINT;}
};

struct A_FLOAT3_UNIT : A_UNIT
{
  REPRESENTATIONAL_TYPE get_representational_type() {return REP_FLOAT;}
  I I_cast();
  std::string to_string() {return to_string(true);}
  std::string to_string(bool last = true);
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARFLOAT;}
};

struct A_RLINK3_UNIT : A_UNIT
{
  // Question. Do we need compiler flag, changing ACCESSOR_MECHANISM_* if != 64-bit arch (_2 for 32-bit)?
  // Answer. Should be OK actually, since we'll just store the 32-bit ones in the 64-bit chunks. 
  std::string to_string();
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_RLINK3;}
}; 

//////////////////////////////////////////////////////////////////////////

struct A_STAMP3_UNIT : A_INT3_UNIT { };

// Some interesting c++ time drafts to look at http://eel.is/c++draft/time
struct A_STAMP_DATETIME_UNIT     : A_STAMP3_UNIT { };
struct A_STAMP_YEAR_UNIT         : A_STAMP3_UNIT { };
struct A_STAMP_MONTH_UNIT        : A_STAMP3_UNIT { };
struct A_STAMP_DAY_UNIT          : A_STAMP3_UNIT { };
struct A_STAMP_HOUR_UNIT         : A_STAMP3_UNIT { };
struct A_STAMP_MINUTE_UNIT       : A_STAMP3_UNIT { };
struct A_STAMP_SECONDS_UNIT      : A_STAMP3_UNIT { };
struct A_STAMP_MILLISECONDS_UNIT : A_STAMP3_UNIT { };
struct A_STAMP_MICROSECONDS_UNIT : A_STAMP3_UNIT { };
struct A_STAMP_NANOSECONDS_UNIT  : A_STAMP3_UNIT { };

struct A_SPAN3_UNIT : A_INT3_UNIT { };

struct A_SPAN_YEAR_UNIT          : A_SPAN3_UNIT
{
  std::string to_string(){return A_INT3_UNIT::to_string() + "y";}
};
struct A_SPAN_MONTH_UNIT         : A_SPAN3_UNIT
{
  std::string to_string(){return A_INT3_UNIT::to_string() + "m";}
};
struct A_SPAN_DAY_UNIT           : A_SPAN3_UNIT
{
  std::string to_string(){return A_INT3_UNIT::to_string() + "d";}
};
struct A_SPAN_HOUR_UNIT          : A_SPAN3_UNIT
{
  std::string to_string(){return A_INT3_UNIT::to_string() + "h";}
};
struct A_SPAN_MINUTE_UNIT        : A_SPAN3_UNIT
{
  std::string to_string(){return A_INT3_UNIT::to_string() + "i";}
};
struct A_SPAN_SECONDS_UNIT       : A_SPAN3_UNIT
{
  std::string to_string(){return A_INT3_UNIT::to_string() + "s";}
};
struct A_SPAN_MILLISECONDS_UNIT  : A_SPAN3_UNIT
{
  std::string to_string(){return A_INT3_UNIT::to_string() + "t";}
};
struct A_SPAN_MICROSECONDS_UNIT  : A_SPAN3_UNIT
{
  std::string to_string(){return A_INT3_UNIT::to_string() + "u";}
};
struct A_SPAN_NANOSECONDS_UNIT   : A_SPAN3_UNIT
{
  std::string to_string(){return A_INT3_UNIT::to_string() + "n";}
};

//////////////////////////////////////////////////////////////////////////

struct A_ARRAY : PRESENTED_BASE
{
  REPRESENTATIONAL_TYPE get_representational_type() {return REP_ARRAY;}
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA;}
public:
  std::string to_string();
  std::string to_string_float_array();
  HASH_CPP_TYPE hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed);
  // PUTATIVE
  //??? virtual SLOP array_index(I i) = 0;
};

struct A_ARRAY_0 : A_ARRAY
{
  UC chunk_width_log_bytes() { return 0; }
  I integer_access_raw(V pointer) {return *(int8_t*)pointer;} 
  void integer_write_raw(V pointer, I x) {*(int8_t*)pointer = x;}
  F float_access_raw(V pointer) {return FN;}
  void float_write_raw(V pointer, F x) {return;}
};

struct A_ARRAY_1 : A_ARRAY
{
  UC chunk_width_log_bytes() { return 1; }
  I integer_access_raw(V pointer) {return *(int16_t*)pointer;} 
  void integer_write_raw(V pointer, I x) {*(int16_t*)pointer = x;}
  F float_access_raw(V pointer) {return *(F1*)pointer;}
  void float_write_raw(V pointer, F x) {*(F1*)pointer = x;}
};

struct A_ARRAY_2 : A_ARRAY
{
  UC chunk_width_log_bytes() { return 2; }
  I integer_access_raw(V pointer) {return *(int32_t*)pointer;} 
  void integer_write_raw(V pointer, I x) {*(int32_t*)pointer = x;}
  F float_access_raw(V pointer) {return *(float*)pointer;}
  void float_write_raw(V pointer, F x) {*(float*)pointer = x;}
};

struct A_ARRAY_3 : A_ARRAY
{
  // no change needed if base is _3
};

struct A_ARRAY_4 : A_ARRAY
{
  UC chunk_width_log_bytes() { return 4; }
  I integer_access_raw(V pointer) {return ((SLAB*)pointer)->i;}
  void integer_write_raw(V pointer, I x) {((SLAB*)pointer)->i = x;}
  F float_access_raw(V pointer) {return ((SLAB*)pointer)->f;}
  void float_write_raw(V pointer, F x) {((SLAB*)pointer)->f = x;}
};

struct A_UNTYPED_ARRAY : A_ARRAY_3
{
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA;}
};

struct A_CHAR0_ARRAY : A_ARRAY_0
{ 
  std::string to_string();
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA;}
  bool is_stringish() const {return true;}
};

struct A_INT0_ARRAY : A_ARRAY_0
{
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARINT;}
};

struct A_INT1_ARRAY : A_ARRAY_1
{
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARINT;}
};

struct A_INT2_ARRAY : A_ARRAY_2
{
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARINT;}
};

struct A_INT3_ARRAY : A_ARRAY_3
{ 
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARINT;}
};

struct A_FLOAT1_ARRAY : A_ARRAY_1
{ 
  std::string to_string(){return to_string_float_array();}
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARFLOAT;}
};

struct A_FLOAT2_ARRAY : A_ARRAY_2
{ 
  std::string to_string(){return to_string_float_array();}
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARFLOAT;}
};

struct A_FLOAT3_ARRAY : A_ARRAY_3
{
  std::string to_string(){return to_string_float_array();}
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_VARFLOAT;}
};

struct A_UNTYPED_RLINK3_ARRAY : A_ARRAY_3
{
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_RLINK3;}
};

//////////////////////////////////////////////////////////////////////////

struct A_STAMP_DATETIME_ARRAY     : A_INT3_ARRAY {};
struct A_STAMP_YEAR_ARRAY         : A_INT1_ARRAY {};
struct A_STAMP_MONTH_ARRAY        : A_INT2_ARRAY {};
struct A_STAMP_DAY_ARRAY          : A_INT2_ARRAY {};
struct A_STAMP_HOUR_ARRAY         : A_INT0_ARRAY {};
struct A_STAMP_MINUTE_ARRAY       : A_INT1_ARRAY {};
struct A_STAMP_SECONDS_ARRAY      : A_INT2_ARRAY {};
struct A_STAMP_MILLISECONDS_ARRAY : A_INT2_ARRAY {};
struct A_STAMP_MICROSECONDS_ARRAY : A_INT3_ARRAY {};
struct A_STAMP_NANOSECONDS_ARRAY  : A_INT3_ARRAY {};

//////////////////////////////////////////////////////////////////////////

struct A_UNIT_USING_ARRAY_LAYOUT : A_CHAR0_ARRAY
{
  bool is_unit() {return true;}
  bool is_fixed_width_unit() {return false;}
  std::string to_string() {return "arrayed-unit";}
  HASH_CPP_TYPE hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed);
};

struct A_STRING_UNIT : A_UNIT_USING_ARRAY_LAYOUT
{
  REPRESENTATIONAL_TYPE get_representational_type() {return REP_STRING;}
  std::string to_string();
  bool is_stringish() const {return true;}
};

struct A_BIGINT_UNIT : A_UNIT_USING_ARRAY_LAYOUT
{
  REPRESENTATIONAL_TYPE get_representational_type() {return REP_INT;}
  HASH_CPP_TYPE hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed);
};

struct A_SPAN_DATETIME_UNIT : A_UNIT_USING_ARRAY_LAYOUT
{
  // packed int3 array of 7 int3s, or packed slab4 array of the types: maybe better
  // only print non-zeroes, unless it's all zero, in which case... 0ymdhis or 0y0m0d0h0i0s or I dunno.
  // for addition math etc, do it "over" (over adverb) the slab4 version to do it quickly, or maybe it will just happen at the special unixy-datetime-struct thing anyway, like we had last time in kerf1.
};

//////////////////////////////////////////////////////////////////////////

struct A_UNTYPED_LIST : A_ARRAY_0
{
  // std::string to_string() {return "untyped_list";}
};

struct A_UNTYPED_SLAB4_ARRAY : A_UNTYPED_LIST
{
  // std::string to_string() {return "untyped_slab4_array";}
};

struct A_UNTYPED_JUMP_LIST : A_UNTYPED_LIST
{
  // std::string to_string() {return "untyped_jump_list";}
};

#pragma mark - ///////////////////////////////////////////////////////////////

struct A_GROUPED : A_ARRAY
{
  // Remark. Grouped objects are combinations of smaller non-grouped (LAYOUT) and grouped items who will override certain PRESENTED indexing methods and so on. For instance, an unhashed MAP depends on two sub-arrays, one for keys and one for values.
  // Rule. The number of top-level GROUPED layout children should be roughly constant size: don't pollute the top level. (Additionally, when under about 15 on this we can use SMALL_COUNT for the `->n` ATTRIBUTES when IN_MEMORY+NON_LIST (PREFERRED_MIXED + COUNTED_VECTOR layout). Remark: Limiting GROUPED to 15 LAYOUT subelements is a nice constraint (always use SMALLCOUNT), then we can push say FOLIO lists down into a single sublist instead of letting them pollute top-level. This I think makes other formats nicer as well.
  // Rule. Idea: Inside GROUPED elements we may want to keep the most "static" (unchanging, ungrowing) layout children first and the more dynamic stuff last, for when the list is laid out in an O(n)-append or O(n)-amend format.
  // Rule. Remember please, we need to be able to combine ZIP types and such for use in MAPs, etc, so they should use generalized operators where possible. All GROUPED class operations should use GROUPED-friendly interaction methods (ie, SLOP/PRESENTED methods) except as when such recurses to the LAYOUT methods (building-blocks).
  bool is_grouped() {return true;}
  CHUNK_TYPE_MEMBER presented_chunk_type() {return CHUNK_TYPE_RLINK3;} // NB. this will be overridden by LAYOUT for LIST formats

  virtual I layout_index_of_grouped_metadata(){return 0;}
  virtual I layout_index_of_grouped_indices(){return 1;}
  virtual I layout_index_of_grouped_keys(){return 2;}
  virtual I layout_index_of_grouped_values(){return 3;}
  SLOP grouped_metadata();
  SLOP grouped_indices();
  SLOP grouped_keys();
  SLOP grouped_values();

  SLOP indices();
  SLOP keys();
  SLOP values();

  virtual std::string to_string();
};

#pragma mark - ///////////////////////////////////////////////////////////////

struct A_ERROR : A_GROUPED
{
  bool is_error() const {return true;}
};

#pragma mark - Folio

struct A_FOLIO_ARRAY : A_GROUPED
{ 
  virtual I layout_index_of_grouped_indices(){return 0;} // we'll use for caching size
  virtual I layout_index_of_grouped_values(){return 1;}

  void cow_append(const SLOP &rhs);
  void cow_join(const SLOP &rhs);
  I count();
  SLOP presented_index_one(const SLOP &rhs);

  void cow_amend_one(const SLOP& x, const SLOP& y);
  void cow_delete_one(const SLOP& x);

  // TODO we'll overload our PRESENTED `join`
  // TODO FOLIO builder SLOP(FOLIO_ARRAY);

  // • Folios must handle ARRAYs and TABLEs as children, at least. A folio of tables should behave like a table. This is actually not true: you can have a TABLE with FOLIO columns, right? Question. Or is it necessary to have FOLIO of tables in order to do certain queries? Question. What were some of the problematic queries for PARTABLES in Kerf1? Answer. I don't think it's going to matter honestly. Judged worth taking the risk of ignoring potential problems for now.
  // Idea. Index of boolean into FOLIO that's also a FOLIO with sparse entries. I suppose this doesn't offer much over `whered` boolean vectors which achieve the same sparsity: it would however improve the width of the indexing vector (1 byte each item versus 4 to 8 for really large tables).

  // Remark. FOLIO is an O(k) list of k lists.
  // Idea. layout_append: add a new list, k+1
  //       presented_append: ...add to final list? remains k, k-th list length+1 | or disable?
  //       presented_join: ... join a new list? k+1
  //       length: sum over length of contained lists 
  //       index: calculated position as if k lists were one big one
  //       amend: as `index`
  // Remark. We probably need consideration for disk lists, so we don't do an unintentional append
  // Idea. It might be nice to start FOLIOs with an empty list at the end, for such reasons. Then we can use the rule of always appending to the final list.
  // Remark. P_O_P. You can cache the list sizes if you need to to do O(log k) binsearch on positions. This is a nice cache b/c lists at pos < k [should not] will not change in length, only the last 
  // Remark. The 0th layout item may need to be the cache of k sizes. Then the k lists can start after that
  // Remark. The 1st layout item should be the array of pointers to other lists. Re: We shouldn't have these raw in the layout.
  // Idea. This works for standard INT0_ARRAY promotions and for ZIP_ARRAY promotions. With a FOLIO wrapping everything, we get promotions for free (indexing is maxed at O(4) because the number of promoted vectors is limited). Then we just have a folio of compressed vectors: same story. This is awkward on disk but fine in-memory.
  // Idea. We'd want folio to track sorted_asc/sorted_desc across all of its sublists (+ table subcolumns). This is an easy check. storing it is a little harder. On writes you check to see if the sublist was broken (its attribute removed), then you remove it globally. On appends, same thing, you check that it was preserved.

  // Remember. You put a list of "A_SOCKET_MAPPED_OBJ" inside a FOLIO that points to remote tables and then you can execute queries remotely (you need to do this with one thread per socket-obj, so it happens asynchronously). When the tables are local inside of a folio, that's just a "striped" table or parted or whatever we called it, you don't typically want to multithread this, the local version, unless you're connected to multiple disks. Does it keep the socket open or closed (like http)? Error handling? Timeouts?
  // Observation. it's bad form for a FOLIO to have a FOLIO as an immediate child. this is detectable, and better form is for the FOLIO to split out all the child's elements and insert them as normal members of the parent where the child folio was.
  // Observation. When promoting integer types because an integer append was bigger than the current width supports, we could instead, in O(1) instead of O(n) time, put the old vector inside a FOLIO with a new empty vector (well, size 1) of the larger type. | 2021.01.13 wild stuff. Because our max promotions is currently ε~3, promotion is O(1) and operations on such a list remain O(1). This could be a compiler flag.
  // Observation. folio can also be a list of wrapped-network-handle-to-maybe-table objects, and this is how you get remote db queries

  // POP you can cache the MIN and MAX, especially of sorted columns, like dates; well, that doesn't buy you anything, but then you can store that as a LANDIS/AVL/INDEXED column, well, that doesn't buy you anything either: the derived column would still be bin-searchable. what you could do is have a reverse index (like cpp-table: arr[2021.08.30]=>say 412. but a cpp-table is O(1) speed if converting date to an int - # it isn't because dates need 4 byte ints, so are too big for tables, so even that doesn't work)  on dates, then you get constant time lookup by date. None of this may be necessary: I think it's liably fast enough as is.
};

#pragma mark - Map

struct A_MAP_ABSTRACT : A_GROUPED
{
  // Remark. The simplest map type is a wrapper around two lists: a list of `keys` and a list of `values`, indexing into the map with a key k finds the index i of k in the list of keys and returns the value at the corresponding position i. There are two modifications we can make to maps. The first is an "attribute map", which is a nested submap of additional information. (To avoid infinite recursion you must have a map type which does not immediately contain a sub-map.) The second is an "index" which is the round_up_power_2 sized hashtable index. It consists of an integer list of indices into the keys[/values] list. If h is the non-cryptographic hash of key k modulo the cardinality of the index, the index[h] stores i such that keys[i] == k is the corresponding key, subject to other hashtable concerns such as collision handling. Additionally, we may want to ability to "upgrade" a map which lacks an attribute map or an index into one that has one. This gives us a ternary square of possibilities for map types: {force_will_not_have, currently_no_but_upgradeable_to_yes, currently_yes_has}^2 or the shorter {no,upg,yes}^2. Nine maps is perhaps too many, so we want to select a smaller subset for our needs. I think we can do it with two currently: [(upg,upg), (yes,yes)], forcing both upgrades to happen at the same time (upgrading both is perhaps a separate quality). Later, if we choose, we could add missing map types. Regarding the attribute map, we would upgrade to possessing one in response to attempts to amend the attribute map. For most reads we could pretend to have one and return an empty map which was not actually stored on the data structure. With respect to the index, we would add a missing one once the key list grew in size to a heuristic which supported the use of a space-tradeoff to support constant-time indexing. We ignore the case of keys with pathological length: applications known to use pathologically large keys should specify maps possessing an index such as (yes, yes) [Remark: Even on maps containing hash indices, we still may want to ignore the use of the index below a certain size, ignoring any pathologically long keys.] [Remark. The issue with the large keys is that there are many different large keys, differing in say the last few bits, then you do O(b) comparisons each time, or O(b*n) ~ O(n^2) total, which is more than O(b) or so for an index.]
  // Remark. Ah right, the other problem was we don't have the index code written yet. So we need [upg,upg] -> [yes, upg] -> [yes, yes] for now.
  // Remark on SET. A SET is a UNIQUE list of integer-indexable keys with an optional hash INDEX. The above discussion on upgrades should apply to SET, and the code we write for hashtable maintenance for MAPs should be applicable to SET. We cannot simply subclass MAP from SET because of several differences in semantics. Perhaps a HASHABLE (: A_GROUPED) class which we have both their abstract classes descend from.

  REPRESENTATIONAL_TYPE get_representational_type() {return REP_MAP;}
  virtual SLOP keyed_lookup(const SLOP &rhs) = 0; // eg
  SLOP presented_index_one(const SLOP &rhs);
  virtual I count();
  HASH_CPP_TYPE hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed);

  // PUTATIVE
  virtual bool map_has_metadata() = 0;
  virtual bool map_has_index() = 0;
  virtual bool map_upgrades_metadata() = 0;
  virtual bool map_upgrades_index() = 0;
  virtual bool map_upgrades_together() = 0;
  virtual bool map_can_add_metadata(){return map_upgrades_metadata() && !map_has_metadata();}
  virtual bool map_can_add_index(){return map_upgrades_index() && !map_has_index();}
  I layout_index_of_grouped_metadata(){return 0;}
  I layout_index_of_grouped_indices(){return 1;}
  I layout_index_of_grouped_keys(){return 2;}
  I layout_index_of_grouped_values(){return 3;}
};

struct A_MAP_UPG_UPG : A_MAP_ABSTRACT 
{
  I layout_index_of_grouped_keys(){return 0;}
  I layout_index_of_grouped_values(){return 1;}
  bool map_has_metadata(){return false;}
  bool map_has_index(){return false;}
  bool map_upgrades_metadata(){return true;}
  bool map_upgrades_index(){return true;}
  bool map_upgrades_together(){return false;}

  SLOP key_find(const SLOP &rhs);
  SLOP keyed_lookup(const SLOP &rhs);

  SLOP cowed_insert_replace(const SLOP& x, const SLOP& y);
  void cow_amend_one(const SLOP& x, const SLOP& y);

  void cow_upgrade_to_attribute_map();

  void cow_set_attribute_map(const SLOP &a);
};

struct A_MAP_NO_NO : A_MAP_UPG_UPG
{
  bool map_upgrades_metadata(){return false;}
  bool map_upgrades_index(){return false;}
};

struct A_MAP_YES_UPG : A_MAP_UPG_UPG
{
  // TODO compiled ref count on attr dict

  I layout_index_of_grouped_metadata(){return 0;}
  I layout_index_of_grouped_keys(){return 1;}
  I layout_index_of_grouped_values(){return 2;}
  bool map_has_metadata(){return true;}
  bool map_has_index(){return false;}
  bool map_upgrades_metadata(){return false;}
  bool map_upgrades_index(){return true;}
  bool map_upgrades_together(){return false;}

  SLOP get_attribute_map();

  void cow_upgrade_to_index_map();

  // Remark. Unhashed may promote to HASHED on amend, same as any vector like INT that might promoted to MIXED. Meaning, the head pointer for the object might change. We could also try to retain it and just add the hash at the end (or beginning and move the keys/values forward).

};

struct A_MAP_YES_YES : A_MAP_YES_UPG
{
  I layout_index_of_grouped_metadata(){return 0;}
  I layout_index_of_grouped_indices(){return 1;}
  I layout_index_of_grouped_keys(){return 2;}
  I layout_index_of_grouped_values(){return 3;}
  bool map_has_metadata(){return true;}
  bool map_has_index(){return true;}
  bool map_upgrades_metadata(){return false;}
  bool map_upgrades_index(){return false;}
  bool map_upgrades_together(){return false;}

  // TODO Question. FILLED layout is basically for hashtable (hashset), the hash part, here, and we kept the ->n value in the header, should we have? does it give us anything to get rid of it? because we can get rid of it if we're not using it in the hashtable / or if it's better to have for a value, eg, it gets us to (2^n)-1 and we were at (2^n)-2 or something. I'm guessing not or we would've done that in kerf1 (seems like in kerf1 it counted populated indices/keys, so unlikely). There's a whole thing in the readme on this: "hash index is O(2n) twice necessary space". Remark. I think the trick for hashtables is, get rid of the FILLED-useless-n, and then the 0 index ( or maybe the 0 and the 1 indices) is always "taken" and you skip it. Remark. FILLED-with-no-n: the sort-tree-key-indexer thing would benefit, sounds like - maybe/maybe not, doesn't seem like it uses filled, but it's not clear whether it needs n or not. We'd have to look at it closely later, there's also like a ground thing that points to the root node which may throw in wrench in this. Ah, we might get it because "LANDIS_NODE" is a 2-tuple of ints, so that seems promising to imply there will be sufficient space? Ah, but then the issue with that seems to be that we only add LANDIS_NODEs at new elements are added to the list: so then if this thing is zeroed, it shouldn't matter in the first place, right? Because it's different from a hashtable/hashset. Cheez louise. It's also an issue for (the old version of) hashtables but not sort-index-landis things because the hash tables would round_up_power_2 (#elts) then double again AND it's FILLED AND RANDOMLY POPULATED (hence not zeroed at the end, even if it had used a zeroes-marker for HASH_NULL)...SO it was always ~4x as large on avg. Landis will always be proportional and the end will be zeroed (if I'm looking correctly).
};

#pragma mark - Set

struct A_SET_ABSTRACT  : A_GROUPED
{
  // Remark/Remember/TODO. You APPEND[/JOIN] to a set (in userland) to [putatively] add a new key, whereas with a map you do `m[key]:value` which is AMEND. Question. Right? Or should we just expose an optimized MAP which uses an AFFINE for the VALUES (takes O(1) space if we always use the same item int8_t(0))?
  SLOP presented_index_one(const SLOP &rhs);
  virtual I count();
};
struct A_SET_HASHTABLE : A_SET_ABSTRACT {};
struct A_SET_UNHASHED  : A_SET_ABSTRACT 
{
  // TODO auto-promote to SET_HASHTABLE past a certain point given as a compiler-#define 
  I layout_index_of_grouped_keys(){return 0;}
  SLOP cowed_insert_replace(const SLOP& x, const SLOP& y);
  void cow_append(const SLOP &rhs);
};

#pragma mark - Enum/Intern

struct A_ENUM_INTERN : A_GROUPED
{
  virtual I layout_index_of_grouped_indices(){return 0;} // arbitrary naming convention from kerf1
  virtual I layout_index_of_grouped_keys(){return 1;}

  I count();
  SLOP presented_index_one(const SLOP &rhs);
  void cow_append(const SLOP &rhs);
  void cow_delete_one(const SLOP& x);
  void cow_amend_one(const SLOP& x, const SLOP& y);
  // void cow_join(const SLOP &rhs); // POP there are many such optimizations in kerf1's version

};
#pragma mark - Affine

struct A_AFFINE : A_GROUPED
{
  // Idea. optimized SUM for AFFINE
  // Idea. ADD/SUBTRACT/NEGATE/MULT/etc on AFFINEs can often yield AFFINEs
  // Idea. `Append` should be able to detect when it can simply modify ranges, see readme.txt
  // Idea. A FOLIO of AFFINEs gives a union of ranges/intervals.
  // Idea. Return FOLIO on `delete_one`, maybe `amend_one`
  // Idea. In all other cases, revert to a non-affine list and perform the simple op
  // Idea. Set complements [compound] would be nice, and then when it gets too complicated you can break it down into a regular array. eg, I have an array of everything 1...2^20 except for 3 7 11. That can be stored compactly. We might want fully general interval operations. 

  // • Idea. If we build an AFFINE/TAKE object, where we can store a million zeroes in O(1) space, then we get a SET for free by using a MAP and setting the VALUES grouped element to an AFFINE/TAKE and always storing zeroes. IOW, we replace the O(n) values of the MAP with an O(1) for the set. Of course, you have to detect the repeated add in the append function of the AFFINE/TAKE. Idea. A AFFINE/TAKE/RANGE can be empty, and that's how we'd start the SET VALUES. Idea. AFFINE box for multiplier and addition can have arb. values in it, so not just ints, of any width, but floats of any width and even vectors, lists, and maps. | Affine can story arbitrary base value to do TAKE/REPEAT on say lists in O(1) space | Affine/take can also handle 10 take 1 2 3 by lengthening base vector. Storing round up power two (eg 1 2 3 1) wastes no more than 1x space but gives you a fast modulo right with bitmasks not divided. Should be saved as a POP tho, not necessary
  // • Idea. Whatever the "distributed shell"/`dsh`/distributed xargs command line thing is, we can use the same hosts format. Also, can use that thing to spawn all the kerf2 instances we need.
  // • Remark: We could have a presented type that shifts INTx_ARRAY by a value, so that for instance you can store 1000 1001 1002 as 1000 + 0 1 2 in an INT0_ARRAY. Possibly this duplicates logic we specified for RANGE/AFFINE. Idea. Perhaps "TRANSFORM", which is a two+ item grouped, the first is a collection of transforms to apply to the standard PRESENTED ops (index, append, ...) the second is the basic list storage (eg INT0). This yields for instance RANGE/AFFINE, DECIMAL, etc.
  // • 2020.11.15 I prob. wrote this somewhere else but it's pretty easy to have an object type called an AFFINE or something, eg, doing 1 2 3...n + * 3 + 2 or whatever. you store n, multvalue and plusvalue and can do simple ops. can detect affine a+a which is 2a. this stores the `tils` and so forth that make them instant for large values. oh, right, the most important part is the accessor is then instant and the space is constant even for billion-item arrays. Similarly, there are other (non-affine) constructions like this where constant sized storage can be exploited with some intelligence on top. Ideally, kerf2 makes it easy to incorporate objects like this. Oh, right, so you'd need like something for `i` as well on there, an affine shift (that's how you get 0,0,0; 1,1,1; 1,2,3; 2,4,6) and then an OVERALL affine shift for the whole vector...anyway, i don't have time to separate this now if it's even necessary. it should be simple. ( 10 TAKE 2 is an affine, so is TIL 27 and so on). Interesting question: how would you model say 101010... ? we could have a KERF-LAMBDA-FUNCTION-ATTACHED form of AFFINEs that computes the value on access only, kind of assumes accesses are sparse? | 2021.01.15 Another interesting thing to do here is to do range/subsequence on a separate object X, so for instance we take f(X,a,b) where a and b are keys (array index integers) and a reference is stored to external object X and we can index into the subsequence of X and iterate over it and what not. Is this even useful?

  // Observation. Odometer, permuations, combinations, are AFFINE-like functions in the sense that their members are generated from functions and need not be stored (`symbolic`-like). However, this is unlikely to realize any savings since indexing on such an object produces a result on the order of what was to be saved.

  // {length=0, base=0, imult=1}
  I count();
  SLOP presented_index_one(const SLOP &rhs);
};

#pragma mark - Stride Array

struct A_STRIDE_ARRAY : A_GROUPED
{
  // Remark. There are obv. similarities with LAYOUT_COUNTED_JUMP_LIST (here
  // "JUMP_LIST") I think the issue why we can't use JUMP_LIST for everything
  // is that when we write such a thing to disk, we want it as (basically,
  // ignoring the file header/container) two files: one file for the jumps
  // integer vector and one file for the char (LIST) payload data containing
  // the subelements. They need to be split otherwise you don't get O(1)
  // appends (the jumps vector overruns into the payload data. you can expand
  // internally in the same file, but you get a "sparse file" (zeroes hole
  // punch) bubble, which people complained about in kerf1, and which is
  // currently only supported on certain Linux filesystems). You could
  // potentially get around this by using RLINK units in place of the two
  // children in JUMP_LIST (to use pointers to separate objects/files),
  // however, without looking at it too closely, I think this could break some
  // of our LAYOUT assumptions; based on what I saw it would need some
  // rewriting, and possibly significant rewriting of LAYOUT, assuming the
  // assumptions work (verifying them could be a heavy lift). STRIDE_ARRAY on
  // the other hand we know will work and it doesn't take long to write.
  // Perhaps we would go back and obviate JUMP_LIST using STRIDE_ARRAY: the
  // unjumped LIST version of STRIDE_ARRAY (STRIDE_ARRAY flattened into a LIST
  // without RLINKs) has basically the same performance, because you can still
  // access the jumps vector and payload vector in O(1) time using the sizes
  // recorded in the unjumped LIST.
  // Remark. Another connection is that LAYOUT_COUNTED_JUMP_LIST is a layout,
  // but STRIDE_ARRAY is a GROUPED and a presented type, so is barred from
  // certain layout operations: I think this connection is tenuous though.
  // Remark. Possibly there is a connection b/t JUMP_LIST and ZIP_ARRAY, but we
  // don't know yet b/c we haven't done JUMP_LIST
  // NB. The benefit of doing a STRIDE_ARRAY (incl. over a JUMP_LIST) is that
  // you can compact a list of arbitrary ragged objects into a constant number
  // (two) of drive files (data structures) with constant-time append and
  // index. A JUMP_LIST cannot currently (2021.08.31) support constant-time
  // append for reasons mentioned above.
  // Remark. LISTs with irregular stride should be written to drive as a
  // STRIDE_ARRAY, unless it suffices to use s/t with fewer assumptions, eg, if
  // read-only then JUMP_LIST is fine.

  // POP. For STRIDES and CORDS, [and likely several other types including any
  // that uses "indexing" like this,] what we really want for the index list is
  // a special FOLIO that stores all 1byte ints in their own vector, then all
  // 2bytes in their own, then 4bytes in their own, then 8bytes. Call it a ...
  // COMPACT_SIMPLE_INDEXING_FOLIO...I dunno, come up with something better.
  // This is not that hard to do. Just have the presented append function look
  // to see if it can safely add it to the last subarray without promoting it,
  // and if not, first add an UNTYPED_ARRAY subarray to the end and force add
  // to untyped subarrays.

  I layout_index_of_grouped_indices(){return 0;}
  I layout_index_of_grouped_values(){return 1;}
  I count();
  void cow_append(const SLOP &rhs);

  SLOP presented_index_one(const SLOP &rhs);
  void cow_amend_one(const SLOP& x, const SLOP& y);
  void cow_delete_one(const SLOP& x);
};

#pragma mark - Cord Array

struct A_CORD_ARRAY : A_GROUPED
{
  // REMARK. WARNING. you need a way to represent NULL (nil) inside of STRING_CORD, bigint cords, etc (or perhaps, rule out implementing them). It should not collide with the empty string or the zero bigint. This may affect your structure and consequently the operations you can perform downstream (for instance, if you use -1 as an index or a length, it would break other guarantees that the index in nondecreasing, or that you get O(1) look-behind for string length (several ops become O(n) for runs of NULLs).). If you insist on null-terminated strings, then storing a string of length 1 that consists of a '\0'-char byte can represent NULL, without colliding with the empty string. But then null-bytes cannot appear inside of strings in cords. (No idea yet how this applies to BIGINT cords.) You could also have a 1-byte inline overhead for every stored string, not ideal. Idea. However, if you store them null-terminated, then the empty string is a null byte, and the NULL string is no bytes, you can keep null-bytes inside strings, and you get a 1-byte overhead stored at the back of the string. Shrug. (This is all assuming you're doing a stride-array concept with two fields: one intvec of indices [into the data] and one charvec of headerless run-on payload data.) Idea. Question. What if you offset every index to be "+1" so you can use "+0" as a signaler for NULL, as in 3 7 7 7 7 11 12 ["cat","bros", nil, nil, nil, "whey", ""] Answer. I think this works, since lookback is still O(1). amend is O(n) but it's going to be anyway with run-on payload data. You need to store a '\0' zero char for the empty string in the payload data --- that appears to be a good trade-off. The index list has good properties. Idea. Any string that begins with (Alternatively. "consists entirely of", but this seems worse than "has as a prefix") '\0', including empty, is one less, shifted over, so that the empty string "" is NULL/nil, the single null-byte string "\0" is the empty string "", the two byte "\0\0" is the one-byte "\0", "\0\0\0" is "\0\0", and so on, and this also plays well with TAPE_HEAD_VECTOR (you merely increment where it points by one on encountering the prefix). What this breaks however is that the index differentials no longer "properly" represent length, some will be off by as much as 1, this seems acceptable. The length represents the cord storage requirement properly, but not the total number of "ideal" string characters, which is less-than-or-equal and is computed in Θ(n) time; this is probably fine since that sum is not going to make useful sense anyway in the context of NULL and EMPTY.

  // POP Feature. Adding a string or even a charvec to an empty list yields a stride_array (via presented append). later we can optimize this to a STRING_CORD, maybe there's a CHARVEC_CORD. (But see Rule at LAYOUT_BASE::cow_append.) Idea. Probably we just have a general CORD and one of the attributes on it is the header "object" (eg, a string header archetype) and then [aside from the strides] everything is just a flat payload in the CHARVEC payload holder thing. Remark. The cord coercion is actually REPRESENTATIONAL type, you don't want to match on like sort_attr_desc, then you coerce the incoming stuff as flat. Idea. If somebody APPENDS to a CORD, you've got two options, either an attribute on the structure (or compiler attribute) that rejects the append (similar to the drive-useful attribute for int0vecs that won't allow promote), or you can promote it to a STRIDE array (easily) and then add the thing as normal. Question. Also at issue: are we doing a copy each time or returning a TAPE_HEAD_VECTOR that points to the payload? Warning. BIGINT may cause trouble with the cord because of the sign bit, you may have to special case it or subclass it or something. (Make the sign bit a byte.) We maybe should do this anyway, but see remarks on BIGINT elsewhere. For unsigned BIGINT, you get a nice and easy CORD [which actually plays super well with the cord we just describe above, check the sign bit for match]. Idea. So really you get for free unsigned BIGINT cord and NEGATIVE BIGINT cord, but if you want mixed (signed) that's probably a special case. Slightly worse than mixed BIGINT cord is STRIDE_ARRAY of BIGINT, or if you're lucky regular stride width LIST. POP To zip a CORD, you just replace the CHAR0VEC payload data array with a similarly zipped one. The widths array you can leave alone or optionally zip.
  // POP, for above POP, Rule. You can't make layout()->cow_append() turn the layout into a GROUPED. So if you want to optimize STRING_CORD or STRIDE_ARRAY you need to do that at presented()->cow_append() NOT layout()->cow_append. probably A_UNTYPED_ARRAY::cow_append 
  // case STRING_UNIT:
  //   // POP Use CORD instead. See remarks at CORD
  //   *parent() = STRIDE_ARRAY;
  //   parent()->append(rhs);
  //   return;
  //   break;

};
struct A_STRING_CORD_ARRAY : A_CORD_ARRAY
{
  // Note. CHARVEC_CORD is so similar you might want to do it at the same time as STRING_CORD

  // Remember. We need some kind of method where if you attempt to add a non-string to the STRING_CORD, it promotes it to something else (stride array? untyped list? FOLIO?) and goes on from there 

};
struct A_MIXED_SIGN_BIGINT_CORD_ARRAY : A_CORD_ARRAY
{
  // WARNING. needs the same ability to represent NULL as STRING_CORD possesses

};

#pragma mark - Zip Array

struct A_ZIP_ARRAY : A_GROUPED
{ 


};

#pragma mark - Memory Mapped

struct A_RWLOCKER_GROUPED : A_GROUPED
{
  // NB. We'll use this to produce locked (default) and unlocked
  // eg
  // cow_append { locking_cow_append }
  // locking_cow_append { sutex.lock_wrapper([] { nonlocking_cow_append  }); 
  // nonlocking_cow_append {}
  // the immediate use is A_MEMORY_MAPPED, but it also gives us locking data structures down the road, and separates the locking logic (making MEMORY_MAPPED slightly less busy)

  // TODO. if member pointers are virtual-safe (they are), and we can pass args (maybe, I don't know how or if yet), we can factor our rwlocker wrapper templates more for MEMORY_MAPPED, eg, to pass around PRESENTED_BASE::xyz style member method pointers.
  // TODO. we might be able to template PRESENTED_BASE vs RWLOCKER (class defns). That would at least cause an error if a method was missing in RWLOCKER that was added to the PRESENTED_BASE
  // TODO. migrate MEMORY_MAPPED rwlocker_wrapper methods to RWLOCKER not MEMORY_MAPPED
  // TODO. to finish migrating the wrappers to here from MEMORY_MAPPED, one issue is we haven't declared how to handle pointers in a regular RWLOCKER, they may need to be parented/managed here or something as part of a dictionary (in order to avoid cpp_arena refcounting hitting 2 and then a cow occuring each time, the alternative is finding a TRANSIENT somehow). Then inside the wrappers we have two different behaviors for SLOP object/PRESENTED_BASE pointer. (The MEMORY_MAPPED version maintains a void* to a mmaped drive item that it manages behind the scenes, with a TRANSIENT arena.) We may need to pass a reference to some allocated SLOP storage for populating, and then clear it (we don't want the pointer to presented_base to disappear before the lambda can make use of it). Or maybe avoid this track altogether.

  virtual bool is_currently_ready() {return false;}
  virtual void* get_good_object_pointer() {return nullptr;}
  virtual bool does_mmap_and_munmap_pointer() {return false;}
};

struct A_MEMORY_MAPPED : A_RWLOCKER_GROUPED
{
  // TODO we need some kind of write locks on here (sutex?) b/c currently it assigns the pointer but doesn't have any locks, any EARLY_Q at least will interfere, EQ makes a SLOP that would increase refcount on cpp_arena SLAB, so we may need write-lock on that

  // Remark certain items on A_MEMORY_MAPPED you can modify without cow even if the refcount is > 1. For instance, you can toggle the mmapping on/off, and you can modify the index for early release into the queue.

  // Idea. For cow_append, obv. we write lock it, but we overload the presented/layout methods to accept a third, default nullptr, ptr to A_MEMORY_MAPPED: when present, it is used to expand the slab, IF it is top-level. (Feature. You can expand nested slabs later, with extra difficulty. But supposedly we don't genuinely need this with the way we expand directory/multifile objects to the drive.)

  // Idea. We can have a separate area for storing multiple sutexes (locks) to simplify things: then you don't need nested/unnested read/write locks to handle wrapped mapping in the pointer and the wrapper actual calls.

  // Feature A_MEMORY_MAPPED(headerless) join: CHAR0_ARRAY -> DRIVE-based cow_layout_append (normally would not work b/c of TAPE_HEAD_VECTOR)

  // Principal: Transmissible over the network
  // Derived: Intransmissible over the network, determined/cached from Principal options
  static const int PRINCIPAL_COUNT = 32;
  typedef enum MEMORY_MAPPED_ATTRIBUTE_MEMBER : UC {
    MEMORY_MAPPED_ATTRIBUTE_PRINCIPAL = 0,
    ///////////////////////////////////
    MEMORY_MAPPED_ATTRIBUTE_PATH = 0,                 // NB. We want relative links preserved as-is for cross-host purposes
    MEMORY_MAPPED_ATTRIBUTE_DRIVE_ACCESS_KIND,
    MEMORY_MAPPED_ATTRIBUTE_OFFSET_OF_OBJECT,         // int64_t
    MEMORY_MAPPED_ATTRIBUTE_HEADERLESS,               // Feature. file_mapped_as_headerless_charvec_not_as_object (bool). This is done in the regular way. get the filelength, MAP_ANON a space equal to the page-rounded length + 1, the +1 is for the fake header, which abuts the right of page 0 (zero-indexed) and contains the char0vec header data. The 0th byte of the file begins on the next page (page 1): to get it there you use MAP_FIXED. Care must be taken when munmapping, mmapping, or expanding/altering the mmapping of this file (for instance, when enlarging the size of the file by appending characters to it). The use of this is for text files, generic data, and such instead of kerf2-format headered binary files. 
    ///////////////////////////////////
    MEMORY_MAPPED_ATTRIBUTE_DERIVED = PRINCIPAL_COUNT,
    ///////////////////////////////////
    MEMORY_MAPPED_ATTRIBUTE_FILE_ORIGIN_POINTER = PRINCIPAL_COUNT, // void*
    MEMORY_MAPPED_ATTRIBUTE_FILESIZE,                              // int64_t
    MEMORY_MAPPED_ATTRIBUTE_INDEX_TO_SELF_IN_EARLY_QUEUE,          // int64_t
    MEMORY_MAPPED_ATTRIBUTE_RLINK_EXPANDED_INITIALIZED,            // bool
    MEMORY_MAPPED_ATTRIBUTE_REDUNDANT_SLOP_WRITE_LOCK_COUNTER,     // int64_t

    ///////////////////////////////////
    ///////////////////////////////////
    MEMORY_MAPPED_ATTRIBUTE_OBJECT_POINTER,           // POP cache, void*, this is origin+offset
    MEMORY_MAPPED_ATTRIBUTE_REALPATH,                 // POP cache, string, to cache realpath() lookup of symlink path
    MEMORY_MAPPED_ATTRIBUTE_DEVICENO_INODENO,         // POP cache, string, avoids a second, and potentially inconsistent!, lookup when we free the object. so perhaps we should store all along. Additionally, it lets us know if the target of the file has changed when we re-map it (which should cause an error, or at least which should trigger an unlock of the old and then a lock of the new). POP. We could detect that MEMORY_MAPPED files in a FOLIO (subarrays) live on different device ids (drives) and then parallelize access to them
    MEMORY_MAPPED_ATTRIBUTE_WAS_DIRECTORY,            // Feature? Useless? bool  - These are actually useful at amend/append time to know how to write the new child to the drive (dir vs multifile)
    MEMORY_MAPPED_ATTRIBUTE_WAS_REGULAR_FILE,         // Feature? Useless? bool 
    MEMORY_MAPPED_ATTRIBUTE_WAS_MULTIFILE,            // Feature? Useless? bool 
    MEMORY_MAPPED_ATTRIBUTE_WAS_PARTED,               // Feature? Useless? bool, dir_contained_parted_dir_so_is_parted
    MEMORY_MAPPED_ATTRIBUTE_FILEHANDLE,               // Feature? int. You could I suppose keep this around *only as long as the origin pointer is valid* and zero it out at the same time. You might use this for file operations while the mmapping is valid
  } MEMORY_MAPPED_ATTRIBUTE; 
  friend inline std::ostream &operator<<(std::ostream &os, MEMORY_MAPPED_ATTRIBUTE c) { return os << static_cast<int>(c); }

  bool is_secretly_a_memory_mapped() {return true;}

  // Remark. We cannot copy the file lock in the case of (MAP_SHARED & PROT_WRITE).
  // Feature. Though we could increment it (unimplemented) in the (PRIVATE & ¬PROT_WRITE) case. In that case, allow_to_copy's return is based on the drive access kind attribute, and you likely need pre- and/or post- copy hooks, to match the dealloc hooks.
  bool allow_to_copy() {return false;}

  virtual I layout_index_of_grouped_metadata(){return 0;}

  bool attribute_is_derived(MEMORY_MAPPED_ATTRIBUTE a) {return a >= MEMORY_MAPPED_ATTRIBUTE_DERIVED;}

  SLOP get_attribute(MEMORY_MAPPED_ATTRIBUTE a);
  void cow_set_attribute(MEMORY_MAPPED_ATTRIBUTE a, const SLOP& x);
  void zero_derived();

  I increment_redundant_write_lock_counter();
  I decrement_redundant_write_lock_counter();

  SLOP get_path();
  DRIVE_ACCESS_KIND get_drive_access_kind();
  bool get_is_headerless();
  I get_offset();

  I get_early_queue_index();

  void* get_bad_file_origin_pointer();
  void* get_good_file_origin_pointer();
  void* get_good_object_pointer();
  SLOP get_good_slop();
  I get_filesize();

  void* good_mmap();
  void safe_good_maybe_munmap(bool queue_forced = false) noexcept;

  bool does_mmap_and_munmap_pointer() {return true;}
  bool is_currently_mmapped() {return !!get_bad_file_origin_pointer();}
  bool does_use_early_queue();
  bool is_currently_ready() {return is_currently_mmapped();}

  void callback_from_mapped_queue_to_store_index(I index);
  void callback_from_mapped_queue_to_inform_removed();

  void dealloc_pre_subelt() noexcept;

  // void cow_append(const SLOP &rhs);
  // SLOP presented_index_one(const SLOP &rhs);
  // I count();
  // F F_cast();
  // I I_cast();

  template<typename L> auto rwlocker_wrapper_read(const L &lambda);
  template<typename L> auto rwlocker_wrapper_write(const L &lambda);
};


#pragma mark - putative / future stuff

struct A_VM : A_GROUPED { };

struct A_LANDIS_THINGY : A_GROUPED
{
  // What does this point to? I don't know. A COMB to a table? Or perhaps the table POINTS to IT, and updates it reactively. Dunno.
  // in kerf1 we used 64-bit ints only, but here we may want to allow 32-bit (you can time this to be sure: how long does a 2B row lookup take with and without an avl tree? this determines the min length (for some array type) that needs a LANDIS). I doubt 16-bits are wide enough to be useful.
};

struct A_LAMBDA : A_GROUPED {};

#pragma mark -
//////////////////////////////////////////////////////////////


PRESENTED_BASE * reconstitute_presented_wrapper(void *presented_vtable, PRESENTED_TYPE presented_type);

}
