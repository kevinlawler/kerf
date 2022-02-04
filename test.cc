namespace KERF_NAMESPACE {

// Remark. An alternative test framework to Google Test is "Catch" (catchorg on github), which is more lightweight and would allow us to remove the dependency on the -lgtest lib. Google Test is nice other than the lib dependency, which we could overcome by compiling within the project. Google Test also has support for parallel running and so on.

// Remark. GTEST_SKIP() << "Skipping single test";

TEST(PrimitiveKerfUnitTests, SlabSize)
{
  ASSERT_TRUE(LOG_SLAB_WIDTH == (UC)log2(sizeof(SLAB))); // nice to have the define for constant expressions
  ASSERT_FALSE(pthread_equal(pthread_self(), NULL_PTHREAD_T));
}

int test_static_asserts()
{
  static_assert(1==1);


  // slab lays out correctly on architecture
  static_assert(16==sizeof(SLAB));

  static_assert(1==sizeof(I0));
  static_assert(2==sizeof(I1));
  static_assert(4==sizeof(I2));
  static_assert(8==sizeof(I3));
  static_assert(2==sizeof(F1));
  static_assert(4==sizeof(F2));
  static_assert(8==sizeof(F3));

  static_assert(sizeof(I3)>=sizeof(V));

  // nothing sneaky going on with `float` on this arch (may not matter anyway)
  static_assert(std::numeric_limits<float>::is_iec559);
  static_assert(24 == std::numeric_limits<float>::digits);

  static_assert(-1==~0, "one's complement integer representation"); // not two's complement
  static_assert(~0 != -INT_MAX, "sign-magnitude integer representation");

  static_assert(-128 == CHAR_MIN);
  static_assert(-128 == INT8_MIN);
  static_assert(   8 == CHAR_BIT);

  static_assert(255LL == ((I)(UC)(((UC)0)-(UC)1)));
  static_assert(0LL == ((I)(UC)((UCHAR_MAX)+(UC)1)));

  // Now: as of C++20 conversion in any direction between unsigned and signed is well-defined and reversible.
  // Before: I was better than UI because conversion I->UI was well-defined by standard but UI->I was implementation-specific/signal.
  static_assert(INT64_MIN == I(UI(INT64_MIN)));
  static_assert(UINT64_MAX == UI(I(UINT64_MAX)));

  static_assert(SLAB_ALIGN==alignof(SLAB));
  static_assert(alignof(SLAB)==std::alignment_of<SLAB>());

  static_assert(offsetof(SLAB, smallcount_list_byte_count) == offsetof(SLAB, smallcount_vector_byte_count));

  static_assert(SUTEX_ROUND_BYTE_WIDTH == sizeof(SUTEX_CONTAINER));
  static_assert(sizeof(SUTEX) == sizeof(SUTEX_CONTAINER));
  static_assert(SUTEX_BIT_LENGTH <= SUTEX_ROUND_BIT_WIDTH);
  static_assert(SUTEX_BIT_OFFSET + SUTEX_BIT_LENGTH + SUTEX_BIT_REMAIN == SUTEX_ROUND_BIT_WIDTH);
  static_assert((-KERF_MAX_NORMALIZABLE_THREAD_COUNT) >= SUTEX_COUNTER_MIN);
  static_assert(SUTEX_MAX_ALLOWED_READERS <= SUTEX_COUNTER_MAX);

  SUCCEED();

  return 0;
}

TEST(PrimitiveKerfUnitTests, StaticAsserts)
{
  ASSERT_EQ(0, test_static_asserts());
}

TEST(PrimitiveKerfUnitTests, PointerCasting)
{
  // pointers cast to int64_t and back correctly on architecture
  char * p1 = reinterpret_cast<char*>(0xffffffffffffffff);
  char * p2 = reinterpret_cast<char*>(0x0000000000000000);
  ASSERT_TRUE(p1 == (char*)(I)p1);
  ASSERT_TRUE(p2 == (char*)(I)p2);
}

TEST(PrimitiveKerfUnitTests, InfAndNullTests)
{
  EXPECT_TRUE(-FI2 == -FI3);
  EXPECT_TRUE(-FI1 == -FI3);
  EXPECT_TRUE(-FI1 == -FI2);
  EXPECT_TRUE( FI2 ==  FI3);
  EXPECT_TRUE( FI1 ==  FI3);
  EXPECT_TRUE( FI1 ==  FI2);
  EXPECT_TRUE(isinf(FI3));
  EXPECT_TRUE(isinf(FI2));
  EXPECT_TRUE(isinf(FI1));
  EXPECT_TRUE(isnan(FN3));
  EXPECT_TRUE(isnan(FN2));
  EXPECT_TRUE(isnan(FN1));
  EXPECT_TRUE( FN3 !=  FN3);
  EXPECT_TRUE( FN2 !=  FN3);
  EXPECT_TRUE( FN2 !=  FN2);
  EXPECT_TRUE( FN1 !=  FN3);
  EXPECT_TRUE( FN1 !=  FN2);
  EXPECT_TRUE( FN1 !=  FN1);

  EXPECT_TRUE(SLOP( IN) == SLOP( FN));
  EXPECT_TRUE(SLOP( II) == SLOP( FI));
  EXPECT_TRUE(SLOP(-II) == SLOP(-FI));
  EXPECT_TRUE(SLOP( II) != SLOP(-II));
  EXPECT_TRUE(SLOP( FI) != SLOP(-FI));
  EXPECT_TRUE(SLOP( IN) == SLOP( IN)) << "NANs are equal in Kerf";
  EXPECT_TRUE(SLOP( FN) == SLOP( FN));
}

TEST(BasicKerfUnitTests, BitInterferenceTests)
{
  SLAB s = {0};

  EXPECT_TRUE(0==s.t_slab_object_layout_type);
  EXPECT_TRUE(0==s.reference_management_arena);
  EXPECT_TRUE(0==s.m_memory_expansion_size);
  EXPECT_TRUE(0==s.r_slab_reference_count);
  EXPECT_TRUE(0==s.a_memory_attribute_reserved);
  EXPECT_TRUE(!s.sutex.writer_waiting);
  EXPECT_TRUE(0==s.sutex.counter);

  s.t_slab_object_layout_type = 1;

  EXPECT_TRUE(1==s.t_slab_object_layout_type);
  EXPECT_TRUE(0==s.reference_management_arena);
  EXPECT_TRUE(0==s.m_memory_expansion_size);
  EXPECT_TRUE(0==s.r_slab_reference_count);
  EXPECT_TRUE(0==s.a_memory_attribute_reserved);
  EXPECT_TRUE(!s.sutex.writer_waiting);
  EXPECT_TRUE(0==s.sutex.counter);

  s.reference_management_arena = (REFERENCE_MANAGEMENT_ARENA_TYPE)1;

  EXPECT_TRUE(1==s.t_slab_object_layout_type);
  EXPECT_TRUE(1==s.reference_management_arena);
  EXPECT_TRUE(0==s.m_memory_expansion_size);
  EXPECT_TRUE(0==s.r_slab_reference_count);
  EXPECT_TRUE(0==s.a_memory_attribute_reserved);
  EXPECT_TRUE(!s.sutex.writer_waiting);
  EXPECT_TRUE(0==s.sutex.counter);

  s.m_memory_expansion_size = 1;

  EXPECT_TRUE(1==s.t_slab_object_layout_type);
  EXPECT_TRUE(1==s.reference_management_arena);
  EXPECT_TRUE(1==s.m_memory_expansion_size);
  EXPECT_TRUE(0==s.r_slab_reference_count);
  EXPECT_TRUE(0==s.a_memory_attribute_reserved);
  EXPECT_TRUE(!s.sutex.writer_waiting);
  EXPECT_TRUE(0==s.sutex.counter);

  s.r_slab_reference_count = 1;

  EXPECT_TRUE(1==s.t_slab_object_layout_type);
  EXPECT_TRUE(1==s.reference_management_arena);
  EXPECT_TRUE(1==s.m_memory_expansion_size);
  EXPECT_TRUE(1==s.r_slab_reference_count);
  EXPECT_TRUE(0==s.a_memory_attribute_reserved);
  EXPECT_TRUE(!s.sutex.writer_waiting);
  EXPECT_TRUE(0==s.sutex.counter);

  s.a_memory_attribute_reserved = 1;

  EXPECT_TRUE(1==s.t_slab_object_layout_type);
  EXPECT_TRUE(1==s.reference_management_arena);
  EXPECT_TRUE(1==s.m_memory_expansion_size);
  EXPECT_TRUE(1==s.r_slab_reference_count);
  EXPECT_TRUE(1==s.a_memory_attribute_reserved);
  EXPECT_TRUE(!s.sutex.writer_waiting);
  EXPECT_TRUE(0==s.sutex.counter);

  s.sutex.writer_waiting = 1;

  EXPECT_TRUE(1==s.t_slab_object_layout_type);
  EXPECT_TRUE(1==s.reference_management_arena);
  EXPECT_TRUE(1==s.m_memory_expansion_size);
  EXPECT_TRUE(1==s.r_slab_reference_count);
  EXPECT_TRUE(1==s.a_memory_attribute_reserved);
  EXPECT_TRUE(s.sutex.writer_waiting);
  EXPECT_TRUE(0==s.sutex.counter);

  s.sutex.counter = 1;

  EXPECT_TRUE(1==s.t_slab_object_layout_type);
  EXPECT_TRUE(1==s.reference_management_arena);
  EXPECT_TRUE(1==s.m_memory_expansion_size);
  EXPECT_TRUE(1==s.r_slab_reference_count);
  EXPECT_TRUE(1==s.a_memory_attribute_reserved);
  EXPECT_TRUE(s.sutex.writer_waiting);
  EXPECT_TRUE(1==s.sutex.counter);

}

TEST(BasicKerfUnitTests, TruthTables)
{
  EXPECT_TRUE( SLOP(1));
  EXPECT_TRUE(!SLOP(0));
  EXPECT_TRUE( SLOP(1.0));
  EXPECT_TRUE(!SLOP(0.0));
  EXPECT_TRUE( SLOP('a'));
  EXPECT_TRUE(!SLOP('\0'));
  EXPECT_TRUE(!SLOP(NIL_UNIT));
}

TEST(BasicKerfUnitTests, IntCastingConversion)
{
  I i = I(SLOP(22));
  EXPECT_EQ(i, 22);
  i = (I)SLOP(22);
  EXPECT_EQ(i, 22);
  // iiiiii = SLOP(22); // this sometimes works depending on whether cast operators are marked `explicit` or not
}

TEST(BasicKerfUnitTests, CompareTests)
{
  EXPECT_FALSE(SLOP(1.2) < SLOP(1));
  EXPECT_TRUE (SLOP(1.2) > SLOP(1));
  EXPECT_TRUE (SLOP(3) == 3);
  EXPECT_FALSE(SLOP(3) == 4);
  EXPECT_TRUE (SLOP(3) == 3.0);
  EXPECT_TRUE (SLOP(1,2) < SLOP(1,3));
  EXPECT_FALSE(SLOP(1,2) > SLOP(1,3));
  EXPECT_FALSE(SLOP(1,3) < SLOP(1,2));
  EXPECT_TRUE (SLOP(1,3) > SLOP(1,2));
}

TEST(BasicKerfUnitTests, ConstructorsAndOpsWork)
{
  ASSERT_EQ('`',   SLOP('`'));
  ASSERT_EQ('\"',  SLOP('\"'));
  ASSERT_EQ((C)127,SLOP((C)127));
  ASSERT_EQ(' ',   SLOP(' '));
  ASSERT_EQ('\'',  SLOP('\''));
  
  SLOP q = SLOP(1) + SLOP(2);

  ASSERT_EQ(1, 1) << "not eq";
  EXPECT_EQ(SLOP(3), q) << "Sum values";

  SLOP r(UNTYPED_ARRAY);
  r.append(10);
  r.append(20);

  EXPECT_EQ(SLOP(10,20), r);

  EXPECT_EQ(SLOP(13,23), q+r);
  EXPECT_EQ(SLOP(13,23), r+q);

  SLOP s(UNTYPED_ARRAY);
  s.append(r);
  s.append(r+100);

  EXPECT_EQ(s,   SLOP(SLOP(10,20),SLOP(110,120)));
  EXPECT_EQ(s+q, SLOP(SLOP(13,23),SLOP(113,123)));
  EXPECT_EQ(q+s, SLOP(SLOP(13,23),SLOP(113,123)));
  
  SLOP gg(INT3_ARRAY);
  EXPECT_EQ(gg, SLOP(UNTYPED_ARRAY));
  

  // Two options: either declare `b` before `a` (the containing/receiving object before the received) or neutralize `a`
  SLOP b(UNTYPED_ARRAY);
  SLOP a = q+s;
  EXPECT_EQ(a, SLOP(SLOP(13,23),SLOP(113,123)));
  b.append(a);
  b.append(a);
  b.append(a);

  SLOP a0 = SLOP(1,SLOP(2, SLOP(3,4)));
  EXPECT_EQ(a0, a0);
  EXPECT_EQ(a0, SLOP(1,SLOP(2, SLOP(3,4))));
  EXPECT_NE(0, SLOP(1,SLOP(2, SLOP(3,4))));
  SLOP(1,SLOP(2, SLOP(3,4)));
  SLOP( SLOP( 70), SLOP( 80.01f, SLOP( 90, 91)));

  const SLOP& one = SLOP(1);
  SLOP trey = SLOP(one,one,one);
  EXPECT_EQ(trey, SLOP(one,one,one));

  EXPECT_EQ(SLOP(1,SLOP(2, SLOP(3,4))), SLOP(1, SLOP(2, SLOP(3,4))));
  EXPECT_EQ(b, SLOP( SLOP(SLOP(13,23),SLOP(113,123)), SLOP(SLOP(13,23),SLOP(113,123)), SLOP(SLOP(13,23),SLOP(113,123))));
  EXPECT_EQ(SLOP(SLOP(SLOP(1,2),3),4), SLOP(SLOP(SLOP(1,2),3),4)); // minimal case [[[1,2],3],4]

  EXPECT_EQ(a+a, SLOP(SLOP(26,46), SLOP(226,246)));

  EXPECT_EQ(15, SLOP(22) - SLOP(7));
  
  EXPECT_EQ(-26, SLOP(13) - SLOP(13) - SLOP(13) - SLOP(13)) << "Subtraction is left-associative";
  EXPECT_EQ(-52, 0 - SLOP(13) - SLOP(13) - SLOP(13) - SLOP(13)) << "Subtraction is left-associative";
  
  EXPECT_EQ(0+a, SLOP(SLOP(13,23),SLOP(113,123)));
  EXPECT_EQ(a+0, SLOP(SLOP(13,23),SLOP(113,123)));
  
  SLOP t(SPAN_YEAR_UNIT, 12);
  EXPECT_EQ(t, 12_y);
  EXPECT_EQ(0_y, t-t);
  EXPECT_EQ(24_y, t+t);
  
  EXPECT_EQ(1.2 + 3.4, SLOP(4.6));
  
  EXPECT_EQ("str", SLOP("str"));
  EXPECT_EQ(SLOP("str"), std::string("str"));
  
  EXPECT_EQ(SLOP("str").append("str"), SLOP("str","str"));
  
  EXPECT_EQ(SLOP(1).join(2).join(r).join(r), SLOP(1, 2, 10, 20, 10, 20));
  EXPECT_EQ(SLOP(1).join(2).join(r).join(r).join(r+r), SLOP(1, 2, 10, 20, 10, 20, 20, 40));
  EXPECT_EQ(SLOP(1).join(2).join(r).join(r).join(r+r).append(r+r), SLOP(1, 2, 10, 20, 10, 20, 20, 40, SLOP(20, 40)));
  
  EXPECT_TRUE(SLOP(101,202,303,404,'a','b'));
  EXPECT_TRUE(SLOP(1,2,3, 'a', "bcd"));
  EXPECT_EQ(SLOP(51,62,73), SLOP(1,2,3) + SLOP(50,60,70));
  EXPECT_EQ(SLOP(2).enlist(), SLOP(UNTYPED_ARRAY).append(2));
  
  
  EXPECT_EQ((SLOP{1,2}), SLOP(1,2));
  // Feature. SLOP(2,3,{1,2,3}) // for some reason this converts/decays it to a (int*) ... why? can we prevent decay?
  // EXPECT_EQ(SLOP(2,3,SLOP(1,2,3)), SLOP(2,3,(int[3]){1,2,3}));
  // EXPECT_EQ(SLOP({10,20,30},{40,50,60}), SLOP(SLOP(10,20,30),SLOP(40,50,60)));

  const int iii3[3] = {1,2,3};
  EXPECT_EQ(SLOP(1,2,3), SLOP(iii3));
  EXPECT_EQ(SLOP(iii3), SLOP({1,2,3}));
  // Feature. EXPECT_EQ(SLOP(2,3,iii3), (SLOP(2,3, (int[3]){1,2,3})));


}

TEST(BasicKerfUnitTests, Adverbs)
{
  auto add = [&](const SLOP& x, const SLOP& y) -> SLOP {return x+y;};
  auto subtract = [&](const SLOP& x, const SLOP& y) -> SLOP {return x-y;};
  auto three = SLOP(1,2,3);

  EXPECT_EQ(SLOP(6), fold(add, SLOP(1,2,3)));
  EXPECT_EQ(SLOP(6), fold(add,three));
  EXPECT_EQ(SLOP(7,8,9), fold(add,three,three));
  EXPECT_EQ(SLOP(1,3,6), unfold(add, SLOP(1,2,3)));
  EXPECT_EQ(SLOP(1,3,6), unfold(add,three));
  EXPECT_EQ(SLOP(SLOP(1, 2, 3), SLOP(2, 3, 4), SLOP(4, 5, 6), SLOP(7, 8, 9)), unfold(add,three,three));
  // TIME( std::cerr <<  "fold(add, range(POW2())): " << (fold(add, range(POW2(20)))) << "\n";)
  // TIME( range(POW2(20)))
  EXPECT_EQ(SLOP(-4,1,1,1), mapback(subtract, range(4), 4));

}

TEST(BasicKerfUnitTests, Verbs)
{
  EXPECT_EQ(SLOP(UNTYPED_ARRAY), range(0));
}

TEST(BasicKerfUnitTests, FolioType)
{
  SLOP fol(FOLIO_ARRAY);
  // // // SLOP fir = fol.layout()->operator[](0);
  // // // fir.inspect();
  // // // SLOP qqq(444);
  // // // fir = qqq;
  // // // fir = SLOP(333);
  // // // DO(3, fir.append(i))
  // // // DO(POW2(0),fol.layout()->operator[](0).append(55))
  // // // DO(POW2(1),fol.layout()->operator[](1).append(22))
  // // // SLOP sec = fol.layout()->operator[](1);
  // // // DO(3, sec.append(4))
  // // // fol.append(99);
  // // // std::cerr << "fol: " << (fol) << "\n";
  // // // // fir.inspect();
  // // // // SLOP(1).inspect();

  fol.join(SLOP(1,2,3));
  fol.join(SLOP(7,8,9));

  EXPECT_EQ(SLOP(1,2,3,7,8,9), fol);

  EXPECT_EQ(6, fol.count());
  EXPECT_EQ(1, fol[0]);
  EXPECT_EQ(3, fol[2]);
  EXPECT_EQ(7, fol[3]);
  EXPECT_EQ(9, fol[5]);
}

TEST(BasicKerfUnitTests, MapType)
{
  // currently takes 100ms unoptimized to pairwise sum two vectors [0, ..., 1million] using default iterators and so on. seemingly all of this is in the iteration. similar timings for vectors of take(1million, 1). optimizing these should take them to optimal (eg, < 10ms. prob. 10ms -> 1ms for int3 -> int0, potentially less for stack-owned memory that we can reuse). See remarks on `+=` versus `+`. (The upshot is the stock path is only 10x slower than the [projected] hand-optimized path. This is good, because that's not bad [in terms of expected performance]. Bad would be closer to 1000x maybe or 100x maybe.)
  // I n = 1000000;
  // SLOP an(UNTYPED_ARRAY);
  // SLOP bn(UNTYPED_ARRAY);
  // DO(n, an.append(i); bn.append(i));
  // TIME(an+bn)

  SLOP map(MAP_UPG_UPG);
  DO(2,map.layout()->operator[](0).append(i))
  DO(2,map.layout()->operator[](1).append(i+100))
  
  // for(const auto& x : map.layout_ref()) std::cerr << "x: " << (x) << "\n";
  // for(const auto& x : map.layout()) std::cerr << "x: " << (x) << "\n";
  // for(const auto& x : map.presented()) std::cerr << "x: " << (x) << "\n";
  // for(const auto& x : range(1))  std::cerr << "x: " << (x) << "\n";
  // for(const auto& x : 10 + range(1).keys())  std::cerr << "x: " << (x) << "\n";
  // for(const auto& x : 100 + range(1).values())  std::cerr << "x: " << (x) << "\n";
  // for(const auto& x : map.values())  std::cerr << "x: " << (x) << "\n";
  // for(const auto& x : map)  std::cerr << "x: " << (x) << "\n";
  // SLOP bigbuild = {}; // standard 5.17.9 "The meaning of x={} is x=T()"
  // std::cerr << "bigbuild: " << (bigbuild) << "\n";
  // std::cerr << "map: " << (map) << "\n";

  EXPECT_EQ(map[map.keys()], map.values());
  EXPECT_EQ(map.keys(), SLOP(0,1));
  
  map.amend_one(0,2);
  map.amend_one(3,4);
  EXPECT_EQ(map.keys(), SLOP(0,1,3));
  EXPECT_EQ(map.values(), SLOP(2,101,4));
  
  EXPECT_EQ(3, map.count());
  EXPECT_EQ(map[1], 101);
}

TEST(BasicKerfUnitTests, MapUpgradesToAttributes)
{
  SLOP v(MAP_UPG_UPG);
  SLOP u(MAP_UPG_UPG);
  u.amend_one("fun", 123);

  EXPECT_NE(MAP_UPG_UPG, MAP_YES_UPG);
  v.amend_one(11, 301);
  v.amend_one(22, 302);
  EXPECT_EQ(v.keys(), SLOP(11,22));
  EXPECT_EQ(v.values(), SLOP(301,302));
  v.presented()->cow_set_attribute_map(u);
  v.amend_one(33,303);
  EXPECT_EQ(MAP_YES_UPG, v.layout()->header_get_presented_type());
  EXPECT_EQ(v.values(), SLOP(301,302,303));
  EXPECT_EQ(v.keys(), SLOP(11,22,33));
}

TEST(BasicKerfUnitTests, AffineRangeType)
{
  SLOP r = SLOP::AFFINE_RANGE(3,3,2);
  EXPECT_EQ(r,SLOP(3,5,7));
  EXPECT_EQ(r+SLOP(1,2,3),SLOP(4,7,10));

  EXPECT_EQ(range(4),SLOP(0,1,2,3));

  auto subtract = [&](const SLOP& x, const SLOP& y) -> SLOP {return x-y;};
  EXPECT_EQ(mapback(subtract, range(4), 4), SLOP(-4,1,1,1));

  I i = 0;
  for (auto &x : range(3))
  {
    EXPECT_EQ(x,i);
    i++;
  }

}

TEST(BasicKerfUnitTests, SetUnhashed)
{
  SLOP s = SET_UNHASHED;
  s.append(10);
  s.append(20);
  s.append(10);
  EXPECT_EQ(s.count(),2);
  A_SET_UNHASHED &p = *(A_SET_UNHASHED*)s.presented();
  p.cowed_insert_replace(30,0);
  EXPECT_EQ(s.count(),3);
}

TEST(BasicKerfUnitTests, EnumType)
{
  SLOP a(UNTYPED_ARRAY);
  SLOP e(ENUM_INTERN);
  EXPECT_EQ(e, a);
  e.append(10);
  a.append(10);
  EXPECT_EQ(e, a);
  e.append(20);
  EXPECT_EQ(e, SLOP(10,20));
  e.append(10);
  e.append(20);
  e.amend_one(0,99);
  EXPECT_EQ(e[0],99);
  EXPECT_EQ(e, SLOP(99,20,10,20));
}

TEST(BasicKerfUnitTests, Hashing)
{
  // TODO make [1,2,3] and [1,2,257] and change 257 to 3 and ensure same hash value but different vector types

  SLOP(NIL_UNIT).hash();
  SLOP(NIL_UNIT, NIL_UNIT).hash();
  SLOP().hash();
  SLOP(UNTYPED_ARRAY).hash();
  SLOP(SLOP(NIL_UNIT).hash());
}

TEST(BasicKerfUnitTests, PRNG)
{
  pcg64 rng(10,20);
  auto r = RNG::get_my_rng();
  r.random_I();
  EXPECT_TRUE(0 <= r.random_F_unit_interval());
  EXPECT_TRUE(r.random_F_unit_interval() < 1);
  EXPECT_EQ(8, sizeof(pcg64::result_type));
  EXPECT_EQ(true, std::is_unsigned<pcg64::result_type>::value);
}

TEST(BasicKerfUnitTests, Sutex)
{
  SLAB s = {0};
  EXPECT_EQ(0, SUTEX::sutex_lock_exclusive(&s.sutex));
  EXPECT_EQ(EDEADLK, SUTEX::sutex_lock_exclusive(&s.sutex));
  SUTEX::sutex_unlock_exclusive(&s.sutex);
  EXPECT_EQ(0, SUTEX::sutex_lock_exclusive(&s.sutex));
  SUTEX::sutex_unlock_exclusive(&s.sutex);
  
  SUTEX::sutex_lock_shared(&s.sutex);
  SUTEX::sutex_lock_shared(&s.sutex);
  SUTEX::sutex_unlock_shared(&s.sutex);
  SUTEX::sutex_unlock_shared(&s.sutex);

  s.slab_lock_safe_wrapper([]{
    EXPECT_TRUE(true);
  });

  BACKOFF().pause().pause();
  kerf_cpu_relax();
}

TEST(BasicKerfUnitTests, StrideArray)
{
  SLOP u(UNTYPED_ARRAY);
  u.append(123);
  u.append(SLOP(222,333,444,555,666,777,888,999,101010));
  auto v = u;
  v.coerce_STRIDE_ARRAY();
  EXPECT_EQ(v,u);
  EXPECT_EQ(v.layout()->header_get_presented_type(), STRIDE_ARRAY);
  EXPECT_NE(u.layout()->header_get_presented_type(), STRIDE_ARRAY);
}

TEST(BasicKerfUnitTests, TapeHeadVector)
{
  char tape_head_test[999] = "the quick brown fox jumps over the lazy dog\0\0\0";
  auto p = SLOP::TAPE_HEAD_VECTOR(tape_head_test, strlen(tape_head_test), CHAR0_ARRAY);
  EXPECT_EQ(p[0],'t');
  EXPECT_EQ(p[1],'h');
  EXPECT_EQ(p[2],'e');
  EXPECT_EQ(p[3],' ');
  EXPECT_EQ(p[4],'q');
}

TEST(MultithreadedKerfUnitTests, SimpleThread)
{
  THREAD t;
  t.detach_state = PTHREAD_CREATE_JOINABLE;
  t.function_to_call = THREAD::thread_noop;
  auto *d = +[](void*)->void* {return (void*)123;}; // try `pause();` + ctrl-c to see SIGUSR2 handler handle dead thread
  t.function_to_call = d;
  t.start();
  __attribute__((no_sanitize_address)) static void *space = nullptr; // Question. Why does the sanitizer fail on this? NB. `static` needed only for __attribute__
  t.join(&space);
}

TEST(MultithreadedKerfUnitTests, SimpleMutex)
{
  MUTEX_UNTRACKED m;
  m.lock_safe_wrapper([]{(void)0;});
  The_Thread_Safe_Early_Remove_Queue.mutex.lock_safe_wrapper([]{(void)0;});
}

TEST(MultithreadedKerfUnitTests, ThreadPool)
{
  THREAD_POOL pool0 = THREAD_POOL<std::function<void(void)>>(1);
  pool0.wait();

  THREAD_POOL pool;
  DO(3, 
        pool.add([i]()->int{
          SLOP a = 99;
          // std::cerr <<  "holler " << i << ": " << (pthread_self()) << "\n"; usleep(5000*1000);
          SLOP b = a + 3;
          return (I)b;
        });
  )

#if THREAD_POOL_SUPPORTS_FUTURES
  auto result = pool.add([]{return pthread_self();});

  auto rg = result.get();
#endif
  pool.wait();

}

TEST(MultithreadedKerfUnitTests, Mapcores)
{
  int n = 64;
return; // BUG 65 abort; plus BUG. mapcores is creating heisenbugs until we make it sensible

  EXPECT_EQ(-range(n), mapcores([&](const SLOP&x) -> SLOP {return -x;} , range(n)));
}

TEST(RegressionUnitTests, IntWideningReadBug)
{
  EXPECT_TRUE(II3 == ACCESSOR_MECHANISM_BASE::prepare_int_for_widening_read(127, 0));
  EXPECT_TRUE(127 == ACCESSOR_MECHANISM_BASE::prepare_int_for_widening_read(127, 1));
}

TEST(RegressionUnitTests, CoerceFlatMapJumpList)
{
  SLOP z = MAP_UPG_UPG;
  z.coerce_FLAT();
  EXPECT_EQ(LAYOUT_TYPE_COUNTED_JUMP_LIST, z.layout()->header_get_slab_object_layout_type());
  EXPECT_EQ(MAP_UPG_UPG, z.layout()->header_get_presented_type());
  EXPECT_EQ(MAP_UPG_UPG, z); // we had an issue with coerce_FLAT
}

TEST(RegressionUnitTests, UntypedPromotion)
{
  SLOP x(UNTYPED_ARRAY);
  x.append(222); // >127
  x.append(333);
  EXPECT_EQ(222, x[0]);

  SLOP u(INT1_ARRAY);
  u.append(130);
  u.append(131);

  SLOP v(MAP_UPG_UPG);
  v.amend_one("arf", 130);
  v.amend_one(22, 131);
  EXPECT_EQ(v.values(), u);
}

TEST(RegressionUnitTests, UntypedArrayAutoSlabReferenceCount)
{
  SLOP a(UNTYPED_ARRAY);
  SLOP x(UNTYPED_ARRAY);
  SLOP y(UNTYPED_ARRAY);

  x.append(y);
  a.append(x[0]);

  SLOP b = x;
  SLOP c = a;

  EXPECT_EQ(x,a);
  EXPECT_EQ(x,b);
  EXPECT_EQ(c,a);

  c.append(a);

  // so we can see the line#'s on failures
  c.neutralize(true);
  b.neutralize(true);
  y.neutralize(true);
  x.neutralize(true);
  a.neutralize(true);
}

TEST(RegressionUnitTests, ParserUntypedArrayReferenceBug)
{
  auto text = "()";
  LEXER l;
  l.lex(text);
  EXPECT_FALSE(l.has_error);

  PARSER par;
  par.parse(l.text, l.tokens);
  EXPECT_FALSE(par.has_error);
}

TEST(RegressionUnitTests, CurlyLambdaParse)
{
  GTEST_SKIP() << "Skipping single test";
  auto text = "{[x]}";
  LEXER l;
  l.lex(text);
  EXPECT_FALSE(l.has_error);

  PARSER par;
  par.parse(l.text, l.tokens);
  EXPECT_FALSE(par.has_error);
}

#if TEST_DRIVE_CASES

TEST(DriveUnitTests, Directory)
{
  std::string path = "_drive_test/_test0.dir";
  SLOP x(10, 20, SLOP(30, 40));

  FILE_OPERATIONS::write_to_drive_path_multifile_expanded(x, path);
  FILE_OPERATIONS::write_to_drive_path_directory_expanded(x, path);

  auto s = FILE_OPERATIONS::read_from_drive_path(path);

  EXPECT_EQ(s, x);

  std::string path2 = "_drive_test/_test1.dir";
  SLOP y = UNTYPED_LIST;
  SLOP z = UNTYPED_SLAB4_ARRAY;
  z.append(400);
  z.append(500);
  z.append(SLOP(600,700,800,900,1000,11000,12000,13000,14000,150000)); //needs to be long enough to not fit in 16bytes

  y.append(SLOP(1,2));
  y.append(3);
  y.append(z);

  FILE_OPERATIONS::write_to_drive_path_directory_expanded(y, path2);
  auto s2 = FILE_OPERATIONS::read_from_drive_path(path2);
  EXPECT_EQ(y, s2);

  FILE_OPERATIONS::write_to_drive_path_directory_expanded(z, path2);
  s2 = FILE_OPERATIONS::read_from_drive_path(path2);
  EXPECT_EQ(z, s2);
}

TEST(DriveUnitTests, MemoryMappedHeaderless)
{
  std::string path = "_drive_test/_test2.dat";
  SLOP(10,20,SLOP(30,40)) >> path;

  auto literal = false;
  auto offset = 0;
  auto headerless = true;
  auto h = FILE_OPERATIONS::memory_mapped_from_drive_path_flat_singlefile(path, DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY, literal, offset, headerless);
  EXPECT_EQ(h[0], (char)200);
}

TEST(DriveUnitTests, EarlyQueue)
{
  // Improvement er, it would be better to test this with a smaller (size<=8 or so) EARLY_QUEUE [versus the standard large size], but I couldn't quickly get it to swap out

  std::string path = "_drive_test/_test3.dat";
  SLOP(10,20,SLOP(30,40)) >> path;

  I n = 4, m = 10;
  THREAD_POOL pool(n);
  DO(n, 
        pool.add([&,i]() {
          DO2(m,
            // auto f = FILE_OPERATIONS::memory_mapped_from_drive_path_flat_singlefile(path, DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY);
            // SLOP q = f[0]+f[1];
  
            auto f = FILE_OPERATIONS::memory_mapped_from_drive_path_flat_singlefile(path, DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY);
            auto g = FILE_OPERATIONS::memory_mapped_from_drive_path_flat_singlefile(path, DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY);
            SLOP q = f[0]+g[1];
  
            // Rule. Now that SLOPs carry the MEMORY_MAPPED lock for the lifetime, we need about threads*active_slops_per_thread available slots in the queue (4*2 here, currently). 
            // TODO ^^ As such now, the above isn't *really* testing the EarlyQueue...you'd need to separate the MM slabs from the locking SLOPs or something like that. Idea. Nested-depth memory_mapped are good enough to solve this issue
            // Duped: Rule. We must have as many available EARLY_QUEUE slots as we have temporary values that require maps (eg, 2 for a dyadic operation).

  
            assert(30==q);
            // std::cerr <<  "thread " << i << ": " << (pthread_self()) << "\n";
            // usleep(10*1000);
            // usleep(1*1000*1000);
            )
        });
  )
  pool.wait();

}

TEST(DriveUnitTests, MemoryMapped)
{
  std::string path3 = "_drive_test/_test4";
  SLOP x0(1,2,3);
  FILE_OPERATIONS::write_to_drive_path(x0, path3, DRIVE_ACCESS_KIND_POSIX_WRITE_FLUSH_SINGLEFILE);
  auto s3 = FILE_OPERATIONS::memory_mapped_from_drive_path(path3);
  EXPECT_EQ(x0, s3);

  auto lit = s3.literal_memory_mapped_from_tracked();
  EXPECT_EQ(lit.layout()->header_get_presented_type(), MEMORY_MAPPED);

  // A_MEMORY_MAPPED &m = *(A_MEMORY_MAPPED*)s.presented();
  // SLOP h = m.get_good_slop();
  // h = NIL_UNIT; // Question. Why is this necessary? Answer. Because we need to get rid of the "good slop" before the owning MEMORY_MAPPED disappears, and with it the validity of the void* we're using here. Update. This is obsolete with "passthrough good slop" MM: eg, we're just returning the data slop instead of the literal MEMORY_MAPPED slab in a slop.

  lit.neutralize(true);
  s3.neutralize(true); // Question. Why is this necessary? Answer. Because otherwise when you blow away the singlefile to make a directory below, when it comes time to deregister this slop it triggers an error. We are attempting to deregister in FILE REGISTRY a mapped singlefile that is now a directory. It likely dies because the path/inode mapping is nonsensical. "Don't do that." See our delete/remove_all functions for more (at FILE_OPERATIONS::ensure_directory_*): if you want you could acquire file locks before deleting. Or, perhaps alternatively, only produce such error messages under DEBUG.

  FILE_OPERATIONS::write_to_drive_path(x0, path3, DRIVE_ACCESS_KIND_WRITE_DIRECTORY);
  s3 = FILE_OPERATIONS::memory_mapped_from_drive_path(path3);
  EXPECT_EQ(x0, s3);
  s3 = NIL_UNIT;
  
  x0 = SLOP(1459, 22, SLOP(33, 44));
  FILE_OPERATIONS::write_to_drive_path(x0, path3, DRIVE_ACCESS_KIND_WRITE_DIRECTORY);
  s3 = FILE_OPERATIONS::memory_mapped_from_drive_path(path3);
  EXPECT_EQ(x0, s3);
  s3.neutralize(true);

  SLOP y = UNTYPED_LIST;
  SLOP z = UNTYPED_SLAB4_ARRAY;
  z.append(400);
  z.append(500);
  z.append(SLOP(600,700,800,900,1000,11000,12000,13000,14000,150000)); //needs to be long enough to not fit in 16bytes
  
  y.append(SLOP(1,2));
  y.append(3);
  y.append(z); // produces BROKEN_CHAIN_RLINK I think

  FILE_OPERATIONS::write_to_drive_path_directory_expanded(y, path3);
  auto s2 = FILE_OPERATIONS::memory_mapped_from_drive_path(path3);
  EXPECT_EQ(y, s2);
  s2.neutralize(true);
  
  FILE_OPERATIONS::write_to_drive_path_directory_expanded(z, path3);
  s2 = FILE_OPERATIONS::memory_mapped_from_drive_path(path3);
  EXPECT_EQ(z, s2);
  s2.neutralize(true);

  auto z3 = SLOP(1,2,SLOP(3,4,SLOP(5,6))); // depth 3
  FILE_OPERATIONS::write_to_drive_path_directory_expanded(z3, path3);
  s2 = FILE_OPERATIONS::memory_mapped_from_drive_path(path3);
  EXPECT_EQ(z3, s2);
  s2.neutralize(true);
}

TEST(DriveUnitTests, Workspace)
{
  SLOP f(11,22,33,SLOP(44,55,SLOP(66,77)));
  std::string mpath = "_drive_test/_mpath";
  f >>= mpath;
  SLOP m = FILE_OPERATIONS::memory_mapped_from_drive_path(mpath);
  SLOP n = m.literal_memory_mapped_from_tracked();
  EXPECT_EQ(true,  m.is_tracking_memory_mapped());
  EXPECT_EQ(false, n.is_tracking_memory_mapped());

  SLOP r = SLOP(m,n,123);
  m.neutralize();
  n.neutralize();
  EXPECT_EQ(false, r[0].is_tracking_memory_mapped());
  EXPECT_EQ(true,  r[1].is_tracking_memory_mapped());

  SLOP q = r;
  EXPECT_EQ(r,q);

  SLOP e(MAP_UPG_UPG);
  e.amend_one("x",10);
  e.amend_one("y",20.0);

  SLOP a(The_Kerf_Tree);
  a.amend_one("arf", 130);
  a.amend_one("bark", 240.5);
  a.amend_one("woof", "string");
  a.amend_one("hoot", e);
  e.neutralize();

  {
    a.amend_one("howl", r);
    SLOP s = a["howl"];
    EXPECT_EQ(false, s[0].is_tracking_memory_mapped());
    EXPECT_EQ(true,  s[1].is_tracking_memory_mapped());
    EXPECT_EQ(f, s[0]);
    EXPECT_EQ(f, s[1]);
    r.neutralize(); q.neutralize(); s.neutralize();
  }

  std::string path = "_drive_test/kerftree.dat";
  a >> path;
  SLOP b{};
  b << path;
  EXPECT_EQ(a,b);

  auto ws = "_drive_test/workspace/";

  FILE_OPERATIONS::workspace_save(ws);
  a.neutralize();
  
  kerf_tree_deinit();
  kerf_tree_init();
  
  SLOP c(The_Kerf_Tree);
  EXPECT_NE(b,c);
  c.neutralize();
  
  FILE_OPERATIONS::workspace_load(ws);

  SLOP d(The_Kerf_Tree);
  EXPECT_EQ(b,d);

  SLOP g = d["howl"];
  EXPECT_EQ(false, g[0].is_tracking_memory_mapped());
  EXPECT_EQ(true,  g[1].is_tracking_memory_mapped());
  EXPECT_EQ(g[0], f);
  EXPECT_EQ(g[1], f);

  g.neutralize();
  d.neutralize();

  // Warning. make sure to deinit and reinit at the end, otherwise you'll leave a MEMORY_MAPPED on the kerf_tree, which is bad for repeated tests, or even for regular use, since we just want a clean tree after tests are done
  kerf_tree_deinit();
  kerf_tree_init();
}

#endif

}
