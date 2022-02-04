#pragma once
namespace KERF_NAMESPACE {

SLOP* PRESENTED_BASE::parent()
{
  return (SLOP*)(((char*)this) - offsetof(SLOP, presented_vtable));
}

LAYOUT_BASE* PRESENTED_BASE::layout()
{
  return (LAYOUT_BASE*)(((char*)this) - offsetof(SLOP, presented_vtable) + offsetof(SLOP, layout_vtable));
  return parent()->layout();
}

auto PRESENTED_BASE::presented()
{
  return this;
}

I PRESENTED_BASE::I_cast()
{
  return this->integer_access_wrapped(layout()->payload_pointer_begin());
}

F PRESENTED_BASE::F_cast()
{
  return this->float_access_raw(layout()->payload_pointer_begin());
}

I A_FLOAT3_UNIT::I_cast()
{
  F x = this->float_access_raw(layout()->payload_pointer_begin());
  if(isinf(x)) return x>0? II : -II;
  if(isnan(x)) return IN;
  return (I)x;
}

F A_INT3_UNIT::F_cast()
{
  I x = this->integer_access_wrapped(layout()->payload_pointer_begin());
  switch(x)
  {
    case  IN: return  FN;
    case  II: return  FI;
    case -II: return -FI;
    default: return (F)x;
  }
}

SLOP PRESENTED_BASE::keys() {return range(this->count());}
SLOP PRESENTED_BASE::indices() {return keys();}
SLOP PRESENTED_BASE::values() {return *parent();}

SLOP PRESENTED_BASE::cowed_insert_replace(const SLOP& x, const SLOP& y)
{
  return 0;
}

#pragma mark -


std::string PRESENTED_BASE::string_for_char(C c, C quote_char, bool less)
{
  if(c==quote_char)
  {
    return "\\" + std::string(1,c);
  }

  switch(c)
  {
    case '"'  : [[fallthrough]];
    case '/'  :  // json escapes forward slashes
                if(less) return std::string(1,c);
                [[fallthrough]];
    case '\\' : return "\\" + std::string(1,c);
                break;
    case '\b' : return "\\b";
    case '\f' : return "\\f";
    case '\n' : return "\\n";
    case '\r' : return "\\r";
    case '\t' : return "\\t";

    default: 
              if(!isprint(c))
              {
                std::string s(64, '\0');
                int written = std::snprintf(&s[0], s.size(), "\\u00%02x", (UC)c);
                s.resize(written);
                return s;
              }
              return std::string(1,c);
  }
}

std::string A_CHAR3_UNIT::to_string()
{
  C c = (C)this->I_cast();
  bool atter = false;
  std::string s = string_for_char(c,*(char*)CHAR_UNIT_QUOTE_ESCAPE_QUOTE, true);
  if(atter && 1==s.size() && s != " " && s != "\"" && s != "'") return "@" + s;
  return CHAR_UNIT_QUOTE_FRONT + s + CHAR_UNIT_QUOTE_BACK;
}

std::string A_INT3_UNIT::to_string()
{
  I i = (I)this->I_cast();
  switch(i)
  {
    case  IN: return "NAN";
    case  II: return "∞";  return "INFINITY";
    case -II: return "-∞"; return "-INFINITY";
    default:  return std::to_string(i);
  }
}

std::string string_for_float(F f, bool force_decimal)
{
  std::string s(64, '\0');
  int written;

  if(isnan(f))
  {
    return "NaN";
  }
  else if(isinf(f))
  {
    if(f>0) return "Infinity";
    else    return "-Infinity";
  }
  else if(force_decimal && 0==f-trunc(f))  // this is for the last elt in a list, in case none showed a decimal point, or for units
  {
    written = std::snprintf(&s[0], s.size(), "%.1f", f); // beats %#g
    s.resize(written);
    return s;
  }

  I w = ACCESSOR_MECHANISM_BASE::minimal_float_log_width_needed_to_store_magnitude(f);

  switch(w)
  {
    default:
    case 3:
      written = std::snprintf(&s[0], s.size(), "%g", f);
      break;
    case 2:
      written = std::snprintf(&s[0], s.size(), "%.*g", FLT_DIG, f);
      break;
    case 1:
      written = std::snprintf(&s[0], s.size(), "%.*g", FLT16_DIG + 1, f); // +1, I think there's a bug in %g or something.
      break;
  }

  s.resize(written);
  return s;
}

std::string A_FLOAT3_UNIT::to_string(bool last)
{
  F f = this->float_access_raw(layout()->payload_pointer_begin());
  return string_for_float(f, last);
}

std::string A_RLINK3_UNIT::to_string()
{
  // more future-friendly, read-only
  return SLOP((SLAB*)this->pointer_access_raw(layout()->payload_pointer_begin())).to_string();
  // more read-only
  return SLOP((SLAB*)*(SLAB**)layout()->payload_pointer_begin()).to_string();
  // more write-enabled
  return SLOP((SLAB**)layout()->payload_pointer_begin()).to_string();
}

std::string A_ARRAY::to_string()
{
  std::string s = "[";

  I i = 0;

  auto g = [&](const SLOP& x)
  {
    if(i>0) s+= ", ";
    s += x.to_string();
    i++;
  };

  switch(layout()->layout_payload_chunk_type())
  {
    case CHUNK_TYPE_VARSLAB:
      parent()->iterator_uniplex_layout_subslop(g); // Θ(n) not Θ(n^2)
      break;
    default:
      LAYOUT_BASE &r = *this->layout();
      I n = layout()->payload_used_chunk_count();
      DO(n, 
          if(i>0) s += ", ";
          SLOP p = r.slop_from_unitized_layout_chunk_index(i);
          s += p.to_string();
      )
      break;
  }

  s += "]";

  return s;
}

std::string A_ARRAY::to_string_float_array()
{
  std::string s = "[";

  I n = layout()->payload_used_chunk_count();
  
  DO(n, 
      if(i>0) s += ", ";
      bool last = false;
      if(i==n-1) last = true;
      SLOP q = layout()->slop_from_unitized_layout_chunk_index(i);
      A_FLOAT3_UNIT* u = (A_FLOAT3_UNIT*)q.presented();
      s += u->to_string(last);
  )

  s += "]";

  return s;
}

std::string A_CHAR0_ARRAY::to_string()
{
  std::string s = CHAR_ARRAY_QUOTE_FRONT;
  
  DO(layout()->payload_used_chunk_count(), s += string_for_char(layout()->slop_from_unitized_layout_chunk_index(i).presented()->I_cast(),*(char*)CHAR_ARRAY_QUOTE_ESCAPE_QUOTE);)

  s += CHAR_ARRAY_QUOTE_BACK;

  return s;
}

std::string A_STRING_UNIT::to_string()
{
  std::string s = STRING_QUOTE_FRONT;
  
  DO(layout()->payload_used_chunk_count(), s += string_for_char(layout()->slop_from_unitized_layout_chunk_index(i).presented()->I_cast(),*(char*)STRING_QUOTE_ESCAPE_QUOTE);)

  s += STRING_QUOTE_BACK;

  return s;

  // quoted, unescaped:
  // return '"' + std::string((const char *)layout()->payload_pointer_begin(), (size_t) layout()->payload_used_chunk_count()) + '"';
}

std::string A_GROUPED::to_string()
{
  std::string g = "{GROUPED:";

  std::ostringstream oss;
  oss << (void*)parent()->slabp;
  std::string s(oss.str());
  g += s;
  g+="->|";
  for(auto& x: layout())
  {
    std::ostringstream oss;
    oss << (void*)x.slabp;
    std::string s(oss.str());
    g += s;
    g += "|";
  }

  g += A_ARRAY::to_string();
  g += "}";
  return g;
}

#pragma mark - Indexing

SLOP PRESENTED_BASE::presented_index_one(const SLOP &rhs)
{
  // Question. How do we reconcile two different SLOP/PRESENTED types with a single class hierarchy? Perhaps a SWITCH() at some level. Can also have the RHS return a function pointer to LHS.
  // if(is_simply_layout())
  return layout()->operator[]((I)rhs.presented()->I_cast());
}

SLOP PRESENTED_BASE::presented_index_many (const SLOP &rhs)
{
  // Plan. pass presented_index_one as a function pointer/lambda do a uniplex iterator (simple return for atom, for lists start with an UNTYPED list builder and cow_append)
  if(rhs.is_unit()) return this->presented_index_one(rhs);

  auto g = [&](const SLOP& x) -> SLOP {return this->presented_index_many(x);};
  return rhs.iterator_uniplex_presented_subslop_accumulator(g);
}

SLOP PRESENTED_BASE::operator[](const SLOP& rhs)
{
  return presented_index_many(rhs);
}

SLOP PRESENTED_BASE::last()
{
  // Observation. More simply described by `return this->operator[](this->count()-1);` PROVIDED we implement null/NAN/nan/* indexing on missing indices
  SLOP d = this->count();
  return d > 0 ? this->operator[](d-1) : NIL_UNIT;
}

#pragma mark - Base

void PRESENTED_BASE::cow_append(const SLOP &rhs)
{
  // Question. Do we need to be concerned about reentrancy issues, if a PRESENTED/LAYOUT calls a parent() SLOP method but it cows in the meantime and rewrites its vtable [to something that may not make sense for earlier embedded calls in the stack stemming from the SLOP]? It's probably OK?
  // grouped exceptions go here
  // TODO a MEMORY_MAPPED object might use different reporting, different expansion
  // TODO this presented_append version needs to check for appends to TENANT/TENANCY objects and reject. Ah, see, I actually don't think we can literally do that, at least not the way we've split memory arenas currently. We can however check for appends to like JUMP_LIST here (and reject? or allow and set flag?) or perform such checks inside the class-level APPENDS in PRESENTED, to, for instance, check for stuff that's not going to work well (eg, a STRIDE_ARRAY in list format)

  layout()->cow_append(rhs);
}

void PRESENTED_BASE::cow_join(const SLOP &rhs)
{
  // // NB. if you want to append `string` rhs instead of appending each char
  // // double-dispatch is actually probably more appropriate here?
  // if(rhs.is_unit()) layout()->cow_join(rhs.enlist());
  // else layout()->cow_join(rhs);

  layout()->cow_join(rhs);
}
 
void PRESENTED_BASE::cow_amend_one(const SLOP& x, const SLOP& y)
{
  layout()->cow_amend_one((I)x.presented()->I_cast(), y);
}

void PRESENTED_BASE::cow_delete_one(const SLOP& x)
{
  layout()->cow_delete_one((I)x.presented()->I_cast());
}

#pragma mark - GROUPED

SLOP A_GROUPED::grouped_metadata()
{
  return layout()->operator[](layout_index_of_grouped_metadata());
}

SLOP A_GROUPED::grouped_indices()
{
  return layout()->operator[](layout_index_of_grouped_indices());
}

SLOP A_GROUPED::grouped_keys()
{
  return layout()->operator[](layout_index_of_grouped_keys());
}

SLOP A_GROUPED::grouped_values()
{
  return layout()->operator[](layout_index_of_grouped_values());
}

SLOP A_GROUPED::indices() { return grouped_indices(); }
SLOP A_GROUPED::keys() { return grouped_keys(); }
SLOP A_GROUPED::values() { return grouped_values(); }

#pragma mark - Folio

void A_FOLIO_ARRAY::cow_amend_one(const SLOP& x, const SLOP& y)
{
  die(FOLIO amend not implemented yet)
}

void A_FOLIO_ARRAY::cow_delete_one(const SLOP& x)
{
  die(FOLIO delete not implemented yet)
}

void A_FOLIO_ARRAY::cow_append(const SLOP &rhs)
{
  // Question. If you want to append to a FOLIO...what do you do? Disable this?
  // Answer. For now, yes, disable
  // Alternative: Allow it but it really messes with the O(1) quality of the count
  die(Cannot `append` to FOLIO (as currently specified))
  return;
}

I A_FOLIO_ARRAY::count()
{
  // `last index` if exists, or 0
  // Remark. factors with FOLIO cow_join

  SLOP last(0);

  SLOP index = grouped_indices();

  SLOP d = index.count();
  if(d > 0)
  {
    last = index[d-1];
  }

  // P_O_P box unbox
  return (I)last;
}

void A_FOLIO_ARRAY::cow_join(const SLOP &rhs)
{

  if(rhs.count() <= 0)
  {
    return;
    // if you want to support this, you need to change the way indices are handled (you'll get dupes inside and we don't handle that currently)
  }

  parent()->cow();

  SLOP last(0);

  SLOP values = grouped_values();
  SLOP index = grouped_indices();
  assert(values.is_tracking_parent_rlink_of_slab() || values.is_tracking_parent_slabm_of_slab());
  assert( index.is_tracking_parent_rlink_of_slab() ||  index.is_tracking_parent_slabm_of_slab());

  SLOP d = index.count();
  if(d > 0)
  {
    last = index[d-1];
  }

  values.append(rhs);
  index.append(rhs.count() + last);

  return;
}

SLOP A_FOLIO_ARRAY::presented_index_one(const SLOP &x)
{
  // P_O_P binary search, possibly, in some cases, for large enough index.count()
  // P_O_P for batch indexing, esp if sorted

  SLOP index = grouped_indices();
  auto k = index.count();

  SLOP before(0);
  I j = 0; // REMARK/TODO may want to factor as "[folio] sublist for index" before doing amend_one?? you need the sublist index AND the (x - `before`) offset'd value

  while(j < k && x >= index[j])
  {
    before = index[j];
    j++;
  }    

  if(j >= k)
  {
    // TODO error or return something
  }

  if(x < 0)
  {
    // TODO error or return something
  }

  return grouped_values()[j][x - before];
}

#pragma mark - Set/Hashset

I A_SET_ABSTRACT::count()
{
  return keys().countI();
}

SLOP A_SET_ABSTRACT::presented_index_one(const SLOP &rhs)
{ 
  return keys()[rhs];
}

SLOP A_SET_UNHASHED::cowed_insert_replace(const SLOP& x, const SLOP& y)
{
  // Remark. duped/factored with A_MAP_*

  SLOP _keys = keys();
 
  SLOP k = _keys.find(x);

  // POP this->count is actually O(n) for some kinds of lists, so we could've cached that key value, or returned sentinel
  if(k >= this->count())
  {
    // 1. Unexisting key, adding new key
    // POP convert to hashed set as size grows
    _keys.append(x);
    return k;
  }
  else
  {
    // 2. Existing key
    return k;
  }
}

void A_SET_UNHASHED::cow_append(const SLOP &rhs)
{
  parent()->cow();
  cowed_insert_replace(rhs, 0);
}

#pragma mark - Keyed Map/Hashtable

I A_MAP_ABSTRACT::count()
{
  return keys().countI();
}

SLOP A_MAP_ABSTRACT::presented_index_one(const SLOP &rhs)
{ 
  return keyed_lookup(rhs);
}

SLOP A_MAP_UPG_UPG::keyed_lookup(const SLOP &rhs)
{
  SLOP keys = grouped_keys();
 
  SLOP k = keys.find(rhs);

  // TODO return value typed by second list ?? this is automatic if you have NAN returns from OOB indexing
  //      compresses to `return grouped_values()[grouped_keys()->find(rhs)]`
  // Idea Possibly this should be a compiler toggle option

  // POP this->count is actually O(n) for some kinds of lists, so we could've cached that key value, or returned sentinel
  if(k >= this->count()) return SLOP(NIL_UNIT);

  SLOP values = grouped_values();

  return values[k];
}

SLOP A_MAP_UPG_UPG::cowed_insert_replace(const SLOP& x, const SLOP& y)
{
  // Remark. duped/factored with keyed_lookup

  SLOP _keys = keys();
 
  SLOP k = _keys.find(x);

  // POP this->count is actually O(n) for some kinds of lists, so we could've cached that key value, or returned sentinel
  if(k >= this->count())
  {
    // 1. Unexisting key, adding new key+value pair
    // POP convert to hashed map as size grows
    _keys.append(x);
    values().append(y);
    return k;
  }
  else
  {
    // 2. Existing key, updating value
    values().amend_one(k, y);
    return k;
  }
}

void A_MAP_UPG_UPG::cow_amend_one(const SLOP& x, const SLOP& y)
{
  parent()->cow();
  cowed_insert_replace(x, y);
}

void A_MAP_UPG_UPG::cow_upgrade_to_attribute_map()
{
  parent()->cow();

  auto value_index = layout_index_of_grouped_values();
  auto key_index = layout_index_of_grouped_keys();

  layout()->cow_append(values());
  layout()->cow_amend_one(value_index, keys());
  layout()->cow_amend_one(key_index, MAP_UPG_UPG); 
  parent()->cowed_rewrite_presented_type(MAP_YES_UPG);
}

SLOP PRESENTED_BASE::get_attribute_map()
{
  return SLOP(MAP_UPG_UPG);
}

void A_MAP_UPG_UPG::cow_set_attribute_map(const SLOP &a)
{
  if(map_can_add_metadata())
  {
    cow_upgrade_to_attribute_map();
    parent()->layout()->cow_amend_one(layout_index_of_grouped_metadata(), a);
  }
}

SLOP A_MAP_YES_UPG::get_attribute_map()
{
  return grouped_metadata();
}

void A_MAP_YES_UPG::cow_upgrade_to_index_map()
{
  parent()->cow();

  auto value_index = layout_index_of_grouped_values();
  auto key_index = layout_index_of_grouped_keys();

  layout()->cow_append(values());
  layout()->cow_amend_one(value_index, keys());
  layout()->cow_amend_one(key_index, INT0_ARRAY); 
  parent()->cowed_rewrite_presented_type(MAP_YES_YES);
}

#pragma mark - Enum/Intern

void A_ENUM_INTERN::cow_append(const SLOP &rhs)
{
  parent()->cow();
  SLOP index = indices();
  index.cow();
  SLOP p = index.presented()->cowed_insert_replace(rhs, 0);
  keys().append(p);
}

SLOP A_ENUM_INTERN::presented_index_one(const SLOP &rhs)
{
  return indices()[keys()[rhs]]; 
}

I A_ENUM_INTERN::count()
{
  return keys().countI();
}

void A_ENUM_INTERN::cow_delete_one(const SLOP& x)
{
  // Remark. This can leave "unattached" items in the hashset. We may/may not want to do anything here. See also kerf1 code.

  parent()->cow();
  keys().delete_one(x);
}

void A_ENUM_INTERN::cow_amend_one(const SLOP& x, const SLOP& y)
{
  parent()->cow();
  SLOP index = indices();
  SLOP _keys = keys();

  // TODO type error !x.is_intish (Alternatively. cast. but watch zeroes)
  // TODO index error  
  if(x >= this->count() || x < 0)
  {
    er(TODO index error)
  }

  index.cow();
  SLOP p = index.presented()->cowed_insert_replace(y, 0);
  _keys.amend_one(x,p);
}

#pragma mark - Affine

I A_AFFINE::count()
{
  return (I)layout()->operator[](0);
}

SLOP A_AFFINE::presented_index_one(const SLOP &rhs)
{
  SLOP length = layout()->operator[](0);
  SLOP base   = layout()->operator[](1);
  SLOP imult  = layout()->operator[](2);

  if(rhs < 0 || rhs > length){ } // TODO error? or weird nan-ish based on `base`?

  return base + (rhs * imult);
}

#pragma mark - Stride Array

I A_STRIDE_ARRAY::count()
{
  return indices().countI();
}

void A_STRIDE_ARRAY::cow_append(const SLOP &rhs)
{
  parent()->cow();

  auto v = values();
  auto w = indices();

  v.layout()->cow_append(rhs);
  I c = v.layout()->payload_bytes_current_used();
  assert(SLAB_ALIGNED(c));
  w.append(c);

  assert(w.layout()->known_flat());
  assert(v.layout()->known_flat());

  return;
}

SLOP A_STRIDE_ARRAY::presented_index_one(const SLOP &x)
{
  // P_O_P: Θ(n). to proceed to O(1) retrieve byte boundaries in indices() then cf. slop_from_unitized_layout_chunk_index for CHUNK_TYPE_VARSLAB
  // TODO type/bounds checking on x
  return values().layout()->operator[](I(x));
}

void A_STRIDE_ARRAY::cow_amend_one(const SLOP& x, const SLOP& y)
{
  // TODO Question. This seems a "canonical" base method for GROUPED, should we have it work as below, generalizing to capture the presented_type for the empty version?
  // Remark. Note that the layout versions inherit layout attributes... we would need to inherit any "presented" attributes into the empty type

  //see LAYOUT_BASE::cow_amend_one(I k, const SLOP &rhs)

  // P_O_P: Θ(n)  Reminder: check attr sorted asc/desc
  // Reminder Don't forget actual cow() when switching to O(1)
  SLOP u(STRIDE_ARRAY);

  I i = 0;

  // TODO type error !x.is_intish (Alternatively. cast. but watch zeroes)
  // TODO index error  
  // TODO we could also ignore all those things silently? what did we do in kerf1?
  if(x >= this->count() || x < 0)
  {
    er(TODO index error)
  }

  I k = I(x);

  auto g = [&](const SLOP& z) { u.presented()->cow_append(k==i++ ? y : z); };
  parent()->iterator_uniplex_layout_subslop(g);

  *parent() = u;

  return;
}

void A_STRIDE_ARRAY::cow_delete_one(const SLOP& x)
{

  // TODO Question. This seems a "canonical" base method for GROUPED, should we have it work as below, generalizing to capture the presented_type for the empty version?
  // Remark. Note that the layout versions inherit layout attributes... we would need to inherit any "presented" attributes into the empty type

  // see LAYOUT_BASE::cow_delete_one(I k)
  // see A_ENUM_INTERN::cow_amend_one(const SLOP& x, const SLOP& y)

  // P_O_P: Θ(n)  Reminder: check attr sorted asc/desc
  // Reminder Don't forget actual cow() when switching to O(1)
  SLOP u(STRIDE_ARRAY);

  I i = 0;

  // TODO type error !x.is_intish (Alternatively. cast. but watch zeroes)
  // TODO index error  
  // TODO we could also ignore all those things silently? what did we do in kerf1?
  if(x >= this->count() || x < 0)
  {
    er(TODO index error)
  }
  I k = I(x);

  auto g = [&](const SLOP& y) { if(i++==k)return; u.presented()->cow_append(y);};
  parent()->iterator_uniplex_layout_subslop(g);

  *parent() = u;
  
  return;
}

#pragma mark - Memory Mapped

SLOP A_MEMORY_MAPPED::get_attribute(MEMORY_MAPPED_ATTRIBUTE a)
{
  return grouped_metadata()[(I)a];
}

void A_MEMORY_MAPPED::cow_set_attribute(MEMORY_MAPPED_ATTRIBUTE a, const SLOP& x)
{
  SLOP m = grouped_metadata();
  m.cow();
  m.presented()->cowed_insert_replace((I)a, x);
}

SLOP A_MEMORY_MAPPED::get_path()
{
  return get_attribute(MEMORY_MAPPED_ATTRIBUTE_PATH);
}

bool A_MEMORY_MAPPED::get_is_headerless()
{
  return (bool)get_attribute(MEMORY_MAPPED_ATTRIBUTE_HEADERLESS);
}

I A_MEMORY_MAPPED::get_filesize()
{
  return (I)get_attribute(MEMORY_MAPPED_ATTRIBUTE_FILESIZE);
}

I A_MEMORY_MAPPED::get_offset()
{
  return (I)get_attribute(MEMORY_MAPPED_ATTRIBUTE_OFFSET_OF_OBJECT);
}

I A_MEMORY_MAPPED::get_early_queue_index()
{
  return (I)get_attribute(MEMORY_MAPPED_ATTRIBUTE_INDEX_TO_SELF_IN_EARLY_QUEUE);
}

DRIVE_ACCESS_KIND A_MEMORY_MAPPED::get_drive_access_kind()
{
  return (DRIVE_ACCESS_KIND)(I)get_attribute(MEMORY_MAPPED_ATTRIBUTE_DRIVE_ACCESS_KIND);
}

bool A_MEMORY_MAPPED::does_use_early_queue()
{
  switch(get_drive_access_kind())
  {
    default:
    case DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE:
    case DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY: 
      return true;
    case DRIVE_ACCESS_KIND_MMAP_PRIVATE_WRITEABLE:
      return false;
  }
}

void* A_MEMORY_MAPPED::get_bad_file_origin_pointer()
{
  return (void*)(I)get_attribute(MEMORY_MAPPED_ATTRIBUTE_FILE_ORIGIN_POINTER);
}

void* A_MEMORY_MAPPED::get_good_file_origin_pointer()
{
  void *p = get_bad_file_origin_pointer();

  if(p == nullptr)
  {
    p = good_mmap(); 
  }

  if(p == nullptr) // still null?
  {
    ERROR(ERROR_MEMORY_MAPPED);
  }

  return p;
}

void* A_MEMORY_MAPPED::good_mmap()
{
  void * _Nullable p = nullptr;

  The_Thread_Safe_Early_Remove_Queue.mutex.lock_safe_wrapper([&]{ // POP locking EQ.mutex is redundant for the mmaps that have !does_use_early_queue()
  parent()->slabp->sutex.sutex_safe_wrapper_exclusive([&]{
    I file_size = 0;

    auto queues = does_use_early_queue();
    DRIVE_ACCESS_KIND access = get_drive_access_kind();
    auto path = (std::string) get_path(); 

    if(queues)
    {
      assert(parent()->layout()->header_get_memory_reference_management_arena() != REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT);
      assert(parent()->slabp != (SLAB*)parent()->auto_slab());

      The_Thread_Safe_Early_Remove_Queue.push_back(parent()->slabp); 

      // Question, does push_back need to be part of the critical section below?
      // Similarly, should push_back mmap in (for does_use_early_queue()
      // satisfying objects) with a callback from the early queue? Otherwise, how
      // do we guarantee it freed on pop? 
      // Answer. No (though it has its own critical wrapper), for the reason that
      // it's OK if it didnt: that means you're one down to begin with (you've
      // "claimed" a file handle but haven't actually used/claimed one with the
      // OS). 
      // Question. is it allowed if we do a big munmap [on another thread's
      // mmapped in MEMORY_MAPPED that gets early removed] with a lot of writes
      // flushed in one of the early removed maps?
      // Answer. A big munmap is OK, under the lock at least, maybe not under
      // critical section, but this is treated elsewhere.
    }

    critical_section_wrapper([&]{
      // POP If there's some weird NAS related issue where this takes too long for a critical section, remove this section and just wrap the `mmap()` portion in a critical section and use pointers [passed to the function] to set the `p` value here locally

      auto pc = path.c_str();

      p = FILE_OPERATIONS::drive_mmap_file_path(pc, &file_size, access);

        if(p != nullptr) // NB. do not error inside critical section
        {
          cow_set_attribute(MEMORY_MAPPED_ATTRIBUTE_FILE_ORIGIN_POINTER, (I)(V)p);
          cow_set_attribute(MEMORY_MAPPED_ATTRIBUTE_FILESIZE,            (I)file_size);
        }
    });
  });
  });

  assert(p || !p); // may be nullptr
  return p;
}

void A_MEMORY_MAPPED::safe_good_maybe_munmap(bool queue_forced) noexcept
{
  The_Thread_Safe_Early_Remove_Queue.mutex.lock_safe_wrapper([&]{ // POP locking EQ.mutex is redundant for the mmaps that have !does_use_early_queue()
  parent()->slabp->sutex.sutex_safe_wrapper_exclusive([&]{
    void *p = get_bad_file_origin_pointer();
    if(!p) return;

    I filesize = get_filesize();

    // critical section?
    // TODO I think this munmap and the early_erase need to be linked somehow: maybe under the lock on EarlyQ, or in a critical section, or using the write-lock sutex on parent()->slabp, or ...?

    // Question. What happens if munmap is interrupted by ctrl-c? It either completes (wishful thinking, but unkown currently) or does a partial unmap. (We have munmap calling msync which can trigger a large write, too large normally for a critical, ie uninterruptible by ctrl-c, section.) Cases: if it completes and handles the write in the background we're good. If it triggers any kind of partial or full msync (async or sync), that's fine, provided it doesn't actually unmap any of the memory pages (again, wishful thinking, but unkown currently). If it does a nonempty partial unmap, that is one or more pages, then we have a problem. Depending on how we treat the origin pointer, we'll either leak the remaining unmapped pages (and consequently an open file handle) (Question. Does it finish writing those nonempty unmapped pages? Remark. I suppose it doesn't matter to us, but it is a new level of inconsistency from interrupted writes (several "single-threaded" "passes" of writes may be pending and lost this way. Remark. Since Linux 2.6.19, MS_ASYNC is in fact a no-op, since the kernel properly tracks dirty pages and flushes them to storage as necessary).), or we'll risk corrupting memory by double unmapping a page which has been reclaimed by some other object allocation. Corrupting memory is not an option. So what's left is:
    //   i. determine we can acceptably leave `munmap` outside a critical section, on the relevant OSes (eg, guaranteed to safely complete in the presence of a signal)
    //  ii. wrap `munmap` in a critical section, accepting any long-ish write/msync delays to ctrl-c, contrary to our ctrl-c philosophy
    // iii. leak the pages and a filehandle ; the question remains on what happens to the writes
    // For now we will do (ii) and mark it a p.o.p. to use (i) or (iii)
    // POP Additionally now, wrapping EARLY_QUEUE.pop_front in a critical section wrapper also/double wraps the munmap here, because the callback there calls this function. If it isn't necessary to wrap munmap here, it won't be there either, and you can move it outside both (in that case you would move the callback outside of critical protection). Remark. (Unrelated to munmap.) Other options besides having EARLY_QUEUE use critical sections is to somehow rewrite the code to be critically safe/always-consistent or to somehow wipe it clean and restart on ctrl-c. Neither seems feasible.
    // P̶O̶P̶.̶ ̶a̶l̶s̶o̶,̶ ̶n̶o̶w̶ ̶t̶h̶a̶t̶ ̶t̶h̶i̶s̶ ̶f̶u̶n̶c̶t̶i̶o̶n̶ ̶(̶c̶u̶r̶r̶e̶n̶t̶l̶y̶ ̶n̶a̶m̶e̶d̶ ̶s̶a̶f̶e̶_̶g̶o̶o̶d̶_̶m̶a̶y̶b̶e̶_̶m̶u̶n̶m̶a̶p̶)̶ ̶h̶a̶s̶ ̶t̶h̶e̶ ̶E̶a̶r̶l̶y̶ ̶Q̶u̶e̶u̶e̶ ̶m̶u̶t̶e̶x̶ ̶l̶o̶c̶k̶ ̶a̶n̶d̶ ̶i̶t̶s̶ ̶o̶w̶n̶ ̶s̶u̶t̶e̶x̶ ̶w̶r̶i̶t̶e̶ ̶l̶o̶c̶k̶ ̶a̶t̶ ̶t̶h̶e̶ ̶b̶e̶g̶i̶n̶n̶i̶n̶g̶,̶ ̶b̶e̶c̶a̶u̶s̶e̶ ̶i̶t̶'̶s̶ ̶b̶e̶i̶n̶g̶ ̶c̶a̶l̶l̶e̶d̶ ̶e̶x̶t̶e̶r̶n̶a̶l̶l̶y̶ ̶b̶y̶ ̶t̶h̶e̶ ̶p̶o̶p̶_̶f̶r̶o̶n̶t̶(̶)̶ ̶o̶n̶ ̶t̶h̶e̶ ̶e̶a̶r̶l̶y̶ ̶q̶u̶e̶u̶e̶,̶ ̶w̶h̶i̶c̶h̶ ̶i̶s̶ ̶w̶r̶a̶p̶p̶e̶d̶ ̶i̶n̶ ̶a̶ ̶c̶r̶i̶t̶i̶c̶a̶l̶ ̶s̶e̶c̶t̶i̶o̶n̶,̶ ̶a̶l̶l̶ ̶o̶f̶ ̶t̶h̶a̶t̶ ̶b̶l̶o̶c̶k̶i̶n̶g̶ ̶n̶o̶w̶ ̶t̶a̶k̶e̶s̶ ̶p̶l̶a̶c̶e̶ ̶i̶n̶ ̶a̶ ̶c̶r̶i̶t̶i̶c̶a̶l̶ ̶s̶e̶c̶t̶i̶o̶n̶.̶ ̶ ̶M̶a̶y̶b̶e̶ ̶w̶e̶ ̶c̶a̶n̶ ̶s̶p̶l̶i̶t̶ ̶i̶t̶ ̶t̶o̶ ̶n̶o̶t̶ ̶d̶o̶ ̶t̶h̶a̶t̶

    // POP switch from method (ii) to (i) or (iii) above. As-is, may result in extended ctrl-c wait times
    critical_section_wrapper([&]{

      {
        // cow_set_attribute(MEMORY_MAPPED_ATTRIBUTE_FILE_ORIGIN_POINTER, (I)nullptr);

        // HACK to replace above ^^ we're doing vv which will update the pointer in-place without rewriting the parent and such. This lets us postpone either rewriting the memory pools to account for returning of data to the "wrong" pool when freed by another thread (as would happen in a callback here) or of updating our amend() methods to be in place versus O(n) rewrites.

        SLOP m = grouped_metadata();
        SLOP k = m.keys();
        SLOP i = k.find((I)MEMORY_MAPPED_ATTRIBUTE_FILE_ORIGIN_POINTER);
        SLOP v = m.values();
        assert(m.layout()->header_get_slab_reference_count() == 1);
        assert(v.layout()->header_get_slab_reference_count() == 1);
        SLOP w = v.layout()->slop_from_unitized_layout_chunk_index((I)i);
        w.slabp->i = 0;
      }

      The_Mmap_Total_Byte_Counter -= filesize;
      The_Munmap_Leak_Tracker++;
      auto r = munmap(p, filesize); 
      The_Munmap_Leak_Tracker--;
      assert(0==r);
    });

    auto queues = does_use_early_queue();
    bool early_release = !queue_forced; // if releasing, released "early", not forced by the EARLY_QUEUE
    if(queues && early_release)
    {
      auto j = get_early_queue_index(); // NB. this must occur paired w/ early_erase inside w/e "lock" we use. Remark. It's probably more sensible to enter it as a callback inside `.early_erase()`, to keep them together
      assert(j >= 0);
      The_Thread_Safe_Early_Remove_Queue.early_erase(j);
    }

  });
  });

}

void* A_MEMORY_MAPPED::get_good_object_pointer()
{
  I offset = get_offset();
  void* p = get_good_file_origin_pointer();

  void *o = ((char*)p) + offset;

  return o;
}

SLOP A_MEMORY_MAPPED::get_good_slop()
{
  if(get_is_headerless())
  {
    I offset = get_offset();
    void* origin = get_good_file_origin_pointer();
    I filesize = get_filesize();

    I length = filesize - offset;

    assert(length >= 0);

    // Remark. Do we need offset for headerless? To do so implies the need for a "width" attribute. But it doesn't seem like we want to return subsections of mapped text files at all. (Mmapped kerf objects have natural ways to determine their "width" (even if in the middle of the file), mapped textfiles don't.) We can respect it for now, but perhaps we should disallow the passing of it on the user side.
    void *ptr = ((char*)origin) + offset;

    PRESENTED_TYPE presented = CHAR0_ARRAY;

    SLOP x = SLOP::TAPE_HEAD_VECTOR(ptr, length, presented);
    return x;
  }

  return SLOP((SLAB*)get_good_object_pointer());
}

void A_MEMORY_MAPPED::callback_from_mapped_queue_to_store_index(I index)
{
  // NB. I don't think this needs a wrapper because we're calling it ourselves [from our own thread]?
  // Question. What if we are mapping in [push_back] while the queue is mapping out [pop_front] (hence a second thread is referring to us)? Answer. This is impossible, because push_back has the mutex on the EARLY_QUEUE, and so would pop_front.
  // Remark. You could instead alter `push_back` to return the integral value, but this outsources handling this lock logic to the calling code. From inside the `push_back` call we can "guarantee" the lock --- sort of. If we end up using other locks elsewhere we may want to revise.
  assert(does_use_early_queue());
  // assert early queue mutex is locked by us
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_INDEX_TO_SELF_IN_EARLY_QUEUE, (I)index);
}

void A_MEMORY_MAPPED::callback_from_mapped_queue_to_inform_removed()
{
  // Remark. Here typically we are being accessed by a foreign thread (though it could be the self thread) and that thread will have the EARLY_QUEUE mutex lock. Provided the mutex is recursive (it is), that means any nested locking mechanisms nested in `good_maybe_unmap`, currently none, will proceed.
  // Note. A full `munmap` [potentially flushing a lot of writes] will take place during the lock on the EARLY_QUEUE mutex. This seems unavoidable.

  assert(does_use_early_queue());
  safe_good_maybe_munmap(true);

  // Question. do we need the early release queue's lock on this? seems like we might. actually, i guess the early queue's method will lock it for us? Answer. No, you don't need the QUEUE's lock, because the calling thread already has it. Currently, we don't even make use of it.
}

void A_MEMORY_MAPPED::zero_derived()
{
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_FILE_ORIGIN_POINTER,               (I)(V)nullptr);
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_FILESIZE,                          (I)0);
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_INDEX_TO_SELF_IN_EARLY_QUEUE,      (I)-1);
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_RLINK_EXPANDED_INITIALIZED,        (I)0);
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_REDUNDANT_SLOP_WRITE_LOCK_COUNTER, (I)0);
  

  ///////
  return;
  ///////

  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_OBJECT_POINTER,               (I)(V)nullptr);
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_REALPATH,                     "");
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_DEVICENO_INODENO,             "");
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_WAS_DIRECTORY,                (I)0);
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_WAS_REGULAR_FILE,             (I)0);
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_WAS_MULTIFILE,                (I)0);
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_WAS_PARTED,                   (I)0);
  cow_set_attribute(A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_FILEHANDLE,                   (I)0);
}

I A_MEMORY_MAPPED::increment_redundant_write_lock_counter()
{
  auto attr = A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_REDUNDANT_SLOP_WRITE_LOCK_COUNTER;
  auto r = (I)get_attribute(attr);
  assert(r>=0);
  ++r;
  cow_set_attribute(attr, (I)r);
  return r;
}

I A_MEMORY_MAPPED::decrement_redundant_write_lock_counter()
{
  auto attr = A_MEMORY_MAPPED::MEMORY_MAPPED_ATTRIBUTE_REDUNDANT_SLOP_WRITE_LOCK_COUNTER;
  auto r = (I)get_attribute(attr);
  assert(r>=1);
  --r;
  cow_set_attribute(attr, (I)r);
  return r;
}

#pragma mark - Memory Mapped (Write)

template<typename L>
auto A_MEMORY_MAPPED::rwlocker_wrapper_write(const L &lambda)
{
  parent()->slabp->sutex.sutex_safe_wrapper_exclusive([&]{
    SLOP x = get_good_slop();
    PRESENTED_BASE* p = x.presented();
    return lambda(p);
  });
}

#pragma mark - Memory Mapped (Read)

template<typename L>
auto A_MEMORY_MAPPED::rwlocker_wrapper_read(const L &lambda)
{
  // Regime. For write locks, as above, you just capture it and then you can mmap in your pointer beforehand without worrying.
  //         For read locks, we check if the pointer is mmap'ped in. If not, we release the lock, do a blocking write lock to mmap it in, and try again.
  //         You cannot upgrade here [from a read lock to a write lock] as you'd like, for more see SUTEX::sutex_upgrade_lock.
  //         (Downgrade probably ok. Write->read, in the second wrapper.)

  decltype(lambda((PRESENTED_BASE*)0)) r;

begin:

  auto succeed = parent()->slabp->sutex.sutex_safe_wrapper_shared([&]{
    if(!is_currently_ready()) return false; // get_good_object_pointer requires a exclusive lock if not ready
    SLOP x = get_good_slop();
    PRESENTED_BASE* p = x.presented();
    r = lambda(p);
    return true;
  });

  // POP you can probably downgrade here from a writelock back to a readlock. Critical section{modify the guard, modify the sutex}. Stay in the same wrapper and call the lambda as above.
  if(!succeed)
  {
    parent()->slabp->sutex.sutex_safe_wrapper_exclusive([&]{
      get_good_object_pointer();
    });
    goto begin;
  }

  return r; 
}

#pragma mark -

void A_MEMORY_MAPPED::dealloc_pre_subelt() noexcept
{
  assert(8==sizeof(*this));
  SLOP path = get_path(); 

  // Observation. If ~SLOP isn't in a critical section, there's no guarantee this is called. You could create a PRESENTED method that fires inside of ~SLOP before the SLOP's cpp workstack deregister, and munmaps things, much like dealloc_pre_subelt kind of does, but this won't get around the problem of leaking the path in The_File_Registry. Ctrl-c is going to leak things from time to time and you should accept that. It's one of our assumptions. (The alternative is to wrap the whole codebase in critical wrappers, and even then some will slip through).

  bool expanded = (bool)get_attribute(MEMORY_MAPPED_ATTRIBUTE_RLINK_EXPANDED_INITIALIZED);

  auto s = get_good_slop();
  auto p = s.presented();
  bool flat = p->layout()->known_flat();

  if(expanded && !flat)
  {
    assert(!get_is_headerless()); // if we can reach here with this somehow, we need to catch it up higher 
    p->parent()->layout_link_parents_lose_references();
    // Feature. zero_pointers [aka don't leave random memory addresses on the drive. this is not necessary but has some benefits as an option]. You can maybe edit `layout_link_parents_lose_references` to have this option with some work. Changing the FILE_OPERATIONS ones is also tricky: the path based one needs the lock, which you won't have, and the deeper SLOP based one assumes the iteration still has live pointers (we either don't here because of layout_link_parents_lose*, or we'd need to free them as part of that [modified] FILE_OPERATIONS::zero_pointers function). Anyway, I'm skipping this for now.
  }
  s.neutralize(true);

  {
    safe_good_maybe_munmap(false);
    if(path != "") The_File_Registry.deregister_path_use(path);
  }
};

#pragma mark - Wrapper Passthroughs

// void A_MEMORY_MAPPED::cow_append(const SLOP &rhs)
// {
//   // NB. We exhibit this as a template for how to do writes, but in actuality all our writes are going to be different because of the differences in how DRIVE objects behave (eg, append() would normally reject on a TRANSIENT arena in a non-MEMORY_MAPPED context, so we need something special to grow it for MAP_SHARED MEMORY_MAPPED appends. Idea./POP. for cases like these that depend on drive type, it might make sense to split into 3+ different types. additionally, the ¬PROT_WRITE MAP_PRIVATE case might benefit from having all of its normal cow write methods disabled with a kerf ERROR instead of a binary crash.) Obsolete. We currently do not use this passthrough pattern.
// 
//   return rwlocker_wrapper_write([&](auto p){ 
//     return p->cow_append(rhs);
//   });
// 
// }
// 
// SLOP A_MEMORY_MAPPED::presented_index_one(const SLOP &rhs)
// {
//   return rwlocker_wrapper_read([&](auto p){ 
//     return p->presented_index_one(rhs);
//   });
// }
// 
// I A_MEMORY_MAPPED::count()
// {
//   return rwlocker_wrapper_read([&](auto p){ 
//     return p->count();
//   });
// }
// 
// I A_MEMORY_MAPPED::I_cast()
// {
//   return rwlocker_wrapper_read([&](auto p){ 
//     return p->I_cast();
//   });
// }
// 
// F A_MEMORY_MAPPED::F_cast()
// {
//   return rwlocker_wrapper_read([&](auto p){ 
//     return p->F_cast();
//   });
// }

#pragma mark -

PRESENTED_BASE * reconstitute_presented_wrapper(void *presented_vtable, PRESENTED_TYPE presented_type)
{
  switch(presented_type)
  {
    default: 
      std::cerr <<  "presented_type: " << (presented_type) << "\n";
      die(Unregistered PRESENTED type passed for vtable)
    case NIL_UNIT:                 new(presented_vtable) A_NIL_UNIT();                 break;
    case CHAR3_UNIT:               new(presented_vtable) A_CHAR3_UNIT();               break;
    case INT3_UNIT:                new(presented_vtable) A_INT3_UNIT();                break;
    case FLOAT3_UNIT:              new(presented_vtable) A_FLOAT3_UNIT();              break;
    case RLINK3_UNIT:              new(presented_vtable) A_RLINK3_UNIT();              break;
    case STAMP_DATETIME_UNIT:      new(presented_vtable) A_STAMP_DATETIME_UNIT();      break; 
    case STAMP_YEAR_UNIT:          new(presented_vtable) A_STAMP_YEAR_UNIT();          break;        
    case STAMP_MONTH_UNIT:         new(presented_vtable) A_STAMP_MONTH_UNIT();         break;       
    case STAMP_DAY_UNIT:           new(presented_vtable) A_STAMP_DAY_UNIT();           break;         
    case STAMP_HOUR_UNIT:          new(presented_vtable) A_STAMP_HOUR_UNIT();          break;        
    case STAMP_MINUTE_UNIT:        new(presented_vtable) A_STAMP_MINUTE_UNIT();        break;      
    case STAMP_SECONDS_UNIT:       new(presented_vtable) A_STAMP_SECONDS_UNIT();       break;     
    case STAMP_MILLISECONDS_UNIT:  new(presented_vtable) A_STAMP_MILLISECONDS_UNIT();  break;
    case STAMP_MICROSECONDS_UNIT:  new(presented_vtable) A_STAMP_MICROSECONDS_UNIT();  break;
    case STAMP_NANOSECONDS_UNIT:   new(presented_vtable) A_STAMP_NANOSECONDS_UNIT();   break; 
    case SPAN_YEAR_UNIT:           new(presented_vtable) A_SPAN_YEAR_UNIT();           break;         
    case SPAN_MONTH_UNIT:          new(presented_vtable) A_SPAN_MONTH_UNIT();          break;        
    case SPAN_DAY_UNIT:            new(presented_vtable) A_SPAN_DAY_UNIT();            break;          
    case SPAN_HOUR_UNIT:           new(presented_vtable) A_SPAN_HOUR_UNIT();           break;         
    case SPAN_MINUTE_UNIT:         new(presented_vtable) A_SPAN_MINUTE_UNIT();         break;       
    case SPAN_SECONDS_UNIT:        new(presented_vtable) A_SPAN_SECONDS_UNIT();        break;      
    case SPAN_MILLISECONDS_UNIT:   new(presented_vtable) A_SPAN_MILLISECONDS_UNIT();   break; 
    case SPAN_MICROSECONDS_UNIT:   new(presented_vtable) A_SPAN_MICROSECONDS_UNIT();   break; 
    case SPAN_NANOSECONDS_UNIT:    new(presented_vtable) A_SPAN_NANOSECONDS_UNIT();    break;  
    case UNTYPED_ARRAY:            new(presented_vtable) A_UNTYPED_ARRAY();            break;
    case CHAR0_ARRAY:              new(presented_vtable) A_CHAR0_ARRAY();              break;
    case INT0_ARRAY:               new(presented_vtable) A_INT0_ARRAY();               break;
    case INT1_ARRAY:               new(presented_vtable) A_INT1_ARRAY();               break;
    case INT2_ARRAY:               new(presented_vtable) A_INT2_ARRAY();               break;
    case INT3_ARRAY:               new(presented_vtable) A_INT3_ARRAY();               break;
    case FLOAT1_ARRAY:             new(presented_vtable) A_FLOAT1_ARRAY();             break;
    case FLOAT2_ARRAY:             new(presented_vtable) A_FLOAT2_ARRAY();             break;
    case FLOAT3_ARRAY:             new(presented_vtable) A_FLOAT3_ARRAY();             break;
    case UNTYPED_RLINK3_ARRAY:     new(presented_vtable) A_UNTYPED_RLINK3_ARRAY();     break;
    case UNTYPED_SLAB4_ARRAY:      new(presented_vtable) A_UNTYPED_SLAB4_ARRAY();      break;
    case STAMP_DATETIME_ARRAY:     new(presented_vtable) A_STAMP_DATETIME_ARRAY();     break;
    case STAMP_YEAR_ARRAY:         new(presented_vtable) A_STAMP_YEAR_ARRAY();         break;
    case STAMP_MONTH_ARRAY:        new(presented_vtable) A_STAMP_MONTH_ARRAY();        break;
    case STAMP_DAY_ARRAY:          new(presented_vtable) A_STAMP_DAY_ARRAY();          break;
    case STAMP_HOUR_ARRAY:         new(presented_vtable) A_STAMP_HOUR_ARRAY();         break;
    case STAMP_MINUTE_ARRAY:       new(presented_vtable) A_STAMP_MINUTE_ARRAY();       break;      
    case STAMP_SECONDS_ARRAY:      new(presented_vtable) A_STAMP_SECONDS_ARRAY();      break;     
    case STAMP_MILLISECONDS_ARRAY: new(presented_vtable) A_STAMP_MILLISECONDS_ARRAY(); break;
    case STAMP_MICROSECONDS_ARRAY: new(presented_vtable) A_STAMP_MICROSECONDS_ARRAY(); break;
    case STAMP_NANOSECONDS_ARRAY:  new(presented_vtable) A_STAMP_NANOSECONDS_ARRAY();  break; 
    case STRING_UNIT:              new(presented_vtable) A_STRING_UNIT();              break;
    case BIGINT_UNIT:              new(presented_vtable) A_BIGINT_UNIT();              break;
    case SPAN_DATETIME_UNIT:       new(presented_vtable) A_SPAN_DATETIME_UNIT();       break;        
    case ZIP_ARRAY:                new(presented_vtable) A_ZIP_ARRAY();                break;
    case FOLIO_ARRAY:              new(presented_vtable) A_FOLIO_ARRAY();              break;
    case UNTYPED_LIST:             new(presented_vtable) A_UNTYPED_LIST();             break;
    case UNTYPED_JUMP_LIST:        new(presented_vtable) A_UNTYPED_JUMP_LIST();        break;
    case MAP_UPG_UPG:              new(presented_vtable) A_MAP_UPG_UPG();              break;
    case MAP_YES_UPG:              new(presented_vtable) A_MAP_YES_UPG();              break;
    case MAP_YES_YES:              new(presented_vtable) A_MAP_YES_YES();              break;
    case SET_UNHASHED:             new(presented_vtable) A_SET_UNHASHED();             break;
    case ERROR_UNIT:               new(presented_vtable) A_ERROR();                    break;
    case ENUM_INTERN:              new(presented_vtable) A_ENUM_INTERN();              break;
    case AFFINE:                   new(presented_vtable) A_AFFINE();                   break;
    case STRIDE_ARRAY:             new(presented_vtable) A_STRIDE_ARRAY();             break;
    case MEMORY_MAPPED:            new(presented_vtable) A_MEMORY_MAPPED();            break;
  }

  return (PRESENTED_BASE*)presented_vtable;
}

} // namespace
