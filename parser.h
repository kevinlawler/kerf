#pragma once

namespace KERF_NAMESPACE
{

#pragma mark - Parser

struct PARSER
{

  bool has_error = false;
  
  void reset_lex_parse_globals()
  {
    LEXER::reset_lex_parse_globals();
  }

  I check_parenthetical_groupings(SLOP &tokens)
  {
    I bad_location = 0;
    //Splitting this out as a pass simplifies
    //other parsing code later
  
    const char* left = lex_classes['g'];
    const char* right = lex_classes['G'];
  
    assert(!strcmp(left, "([{"));
    assert(!strcmp(right,")]}"));
  
    I depth = 0;
    C stack[PARSE_MAX_DEPTH+1] = {0};
    // bool sql_started[PARSE_MAX_DEPTH+1] = {false};

    bool early_break_flag = false;
    bool fail_flag = false;

    tokens.iterator_uniplex_presented_subslop([&](const SLOP& peek){
  
      I kind    = (I)peek[+TOKEN_KIND];
      SLOP payload = peek[+TOKEN_PAYLOAD];
      I token_start = (I)peek[+TOKEN_SNIPPET_START];
      I token_end   = (I)peek[+TOKEN_SNIPPET_END];
  
      bad_location = token_start;
  
      //    //SQL is a "parenthetical" grouping.
      //    if((TOKENS_SQL_START == kind))
      //    {
      //      if(sql_started[depth])
      //      {
      //        //Strictly, I don't think we need to reject on SQL
      //        //nested raw outside of "([{" groups; still, 
      //        //having trouble thinking of a use case and this
      //        //is more expedient for now 2014.12.02
      //        //Full support is tricky.
      //        parse_errno = ERROR_PARSE_NESTED_SQL;
      //        //goto fail;
      //      }
      //
      //      sql_started[depth] = true;
      //    }
  
      if(TOKENS_LEFT != kind && TOKENS_RIGHT != kind)
      {
        return;//ignore most everything
      }
  
      C found = (C)payload[0];
  
      if(TOKENS_LEFT == kind)
      {
        stack[depth++] = found;
  
        if(depth > PARSE_MAX_DEPTH)
        {
          parse_errno = ERROR_PARSE_DEPTH;
          fail_flag = true;
          early_break_flag = true;
          return;
        }
  
        // sql_started[depth] = false;
        assert(*strchr(left, found));
      }
  
      if(TOKENS_RIGHT == kind)
      {
        if(depth <= 0)
        {
          parse_errno = ERROR_PARSE_OVERMATCH;
          fail_flag = true;
          early_break_flag = true;
          return;
        }
  
        //The left item we read in earlier
        C stored = stack[--depth];
  
        //What the left item should be based on
        //the right item we just read
        assert(*strchr(right, found));
        I index = strchr(right, found) - right;
        C expectation = left[index]; 
  
        if(stored != expectation)
        {
          parse_errno = ERROR_PARSE_MISMATCH;
          fail_flag = true;
          early_break_flag = true;
          return;
        }
      }
    }, &early_break_flag);

    if(fail_flag) goto fail;
  
    if(depth > 0)
    {
      parse_errno = ERROR_PARSE_UNMATCH;
      goto fail;
    }
  
  succeed:
    parse_errno = 0;
    parse_errno_location = 0;
    return 0;
  
  fail:
    parse_errno_location = bad_location;
    return -1;
  }

  void mine(SLOP& tokens, I dfas[], I start, I *travelled, I *mark, I *error, I *drop, I *bundle)
  {
    //This is copied from/modeled on LEXing's toke()
    //Factoring them would be possible but counterproductive
  
    I dfa_count = 0;
  
    I *p = dfas;
    while(*p++) dfa_count++;
  
    I position = start;
    *travelled = 0;
    *mark = TOKENS_NONE;
    *bundle = false;
  
    I textual_location = -1;
  
    char prev_states[TOKEN_GROUP_SIZE] = {0};
    char      states[TOKEN_GROUP_SIZE] = {0};
    I    run_lengths[TOKEN_GROUP_SIZE] = {0}; 
  
    I max_run = 0;
    I max_run_index = 0;
    I max_run_state = 0;
  
    while(position < tokens.count())
    {
      SLOP peek = tokens[position];
      I kind = (I)peek[+TOKEN_KIND];
  
      I start = (I)peek[+TOKEN_SNIPPET_START];
      textual_location = start;
  
      I current_max = 0;
      I current_max_index = 0;
      I current_max_state = 0;
  
      //step
      DO(dfa_count, DFA dfa = PARSE_DFAS[dfas[i]]; 
                      char *grid = (char*)dfa.grid;
                      char *state_kinds = (char*)dfa.state_kinds;
                      if(!state_kinds) continue;
                      I before = states[i];
                      prev_states[i] = before;
  
                      states[i] = grid[256*before + kind];
                      run_lengths[i]++;
                      C kind = state_kinds[states[i]];
                      if(0==states[i]) run_lengths[i] = 0;
  
                      //NB: If this causes problems, split into
                      //current_max for INCOMPLETE and ACCEPT states
                      //Then you can always fall back to old accept if exists
                      if(current_max < run_lengths[i]) 
                      {
                        // "<" ensures we take the first token kind in the event of a tie
                        current_max = run_lengths[i];
                        current_max_index = i;
                        current_max_state = states[i];
                      }
      )
  
      if(current_max <= max_run)
      {
        break;
      }
  
      //keep stepping
      max_run = current_max;
      max_run_index = current_max_index;
      max_run_state = current_max_state;
  
      position++;
    }
  
    *travelled = max_run;
  
    if(max_run <= 0)
    {
      return;
    }
  
    I dfa_index = dfas[max_run_index];
    I state = max_run_state;
    C kind = PARSE_DFAS[dfa_index].state_kinds[state];
  
    *bundle = false;//early returns -> no bundling
  
    switch(kind) 
    {
      case 'R':[[fallthrough]];
      case 'I': 
        return; //no error
      case 'E':
        *error=ERROR_PARSE_INCOMPLETE; 
  
        switch(dfas[0]) //slightly hacky
        {
          case TOKEN_GROUP_REJECT_NUM_VAR: *error = ERROR_PARSE_NUM_VAR; break;
        }
  
        parse_errno = *error;
        parse_errno_location = textual_location;
        return;
    }
  
    *mark = PARSE_DFAS[dfa_index].bundle_mark;
    *drop = PARSE_DFAS[dfa_index].bundle_drop;
    *bundle = true;
  
    return;
  }

  SLOP wrangle(SLOP& parent, I start, I length, I drop_inert)
  {
    SLOP list = UNTYPED_ARRAY;
  
    I i = 0;
  
    for(i = start; i < start + length && i < parent.count(); i++)
    {
      SLOP peek = parent[i];
      I kind = (I)peek[+TOKEN_KIND];
      bool is_inert = (kind == TOKENS_WHITESPACE || kind == TOKENS_COMMENT); 
  
      if(drop_inert && is_inert) continue;
  
      list.append(peek);
    }
  
    return list;
  }

  SLOP token_filter_negate_numbers(SLOP& tokens)
  {
    if(tokens.nullish())
    {
      return NIL_UNIT;
    }
    //probably could refactor this as a modified parse DFA
    //but it's already written
  
    SLOP list = UNTYPED_ARRAY;
  
    SLOP dash = SLOP(CHAR0_ARRAY).append('-');
  
    DO(tokens.count(),
        SLOP v = tokens[i];

        if(dash == v[+TOKEN_PAYLOAD])
        {
          if(i < _i - 1)
          {
            SLOP peek = tokens[i+1];
            if((+TOKENS_NUMBER) == peek[+TOKEN_KIND])
            {
              bool grab = false;
  
              if(i==0)
              {
                grab = true;
              }
              else
              {
                SLOP before = tokens[i-1];
                I kind = (I)before[+TOKEN_KIND];
  
                switch(kind)
                {
                  case TOKEN_GROUP_CURLY_BRACE:
                  case TOKEN_GROUP_PLAIN:
                  case TOKEN_GROUP_SEPARATION:
                  case TOKENS_VERB_SYM:
                  case TOKENS_VERB_WORD:
                  case TOKENS_COLON:
                  case TOKENS_COMMENT://?
                  case TOKENS_LEFT:
                  case TOKENS_SEPARATOR:
                  case TOKENS_WHITESPACE:
                    grab = true;
                    break;
                }
  
                if(grab)
                {
                  assert(kind != TOKEN_GROUP_ROUND_PAREN);    // (2+3)-1 is minus
                  assert(kind != TOKEN_GROUP_SQUARE_BRACKET); // ar[0]-1 is minus
                }
              }
  
              if(grab)
              {
                I start = (I)v[+TOKEN_SNIPPET_START];
                I end = (I)peek[+TOKEN_SNIPPET_END];

                SLOP pre = dash.copy_join(peek[+TOKEN_PAYLOAD]);
                // currently we don't have a CHAR0_ARRAY->STRING_UNIT cast yet
                std::string s((const char*)pre.layout()->payload_pointer_begin(), pre.countI());
                SLOP payload(s);
  
                SLOP bundle = LEXER::token_bundle(TOKENS_NUMBER, start, end, payload);
                list.append(bundle);
                i+=1;
                continue;
              }
            }
          }
        }
        list.append(v);
    )

    return list;
  }
  
  SLOP token_filter_drop_whitespace_comments(SLOP& tokens)
  {
    if(tokens.nullish())
    {
      return NIL_UNIT;
    }
    //could refactor this as a modified parse DFA (but why I guess)
  
    SLOP list = UNTYPED_ARRAY;
  
    DO(tokens.count(), SLOP v = tokens[i]; I k = (I)v[+TOKEN_KIND]; if(TOKENS_WHITESPACE==k || TOKENS_COMMENT ==k)continue; list.append(v))
  
    return list;
  }

  SLOP token_filter_group_dfas(SLOP& input_tokens, I dfas[])
  {
    if(input_tokens.nullish())
    { 
      return NIL_UNIT;
    }
  
    I current_start = 0;
  
    SLOP list = UNTYPED_ARRAY;
  
    I travelled;
  
    for(; current_start < input_tokens.countI(); current_start += travelled)
    {
      travelled = 0;
      I mark = 0;
      I error = 0;
      I drop = 0;
      I bundle = 0;
  
      mine(input_tokens, dfas, current_start, &travelled, &mark, &error, &drop, &bundle);
  
      travelled = MAX(1, travelled); //at least 1
  
      if(error)
      {
        //this madness has some useful info in it
        //extract before deleting
        //?????????????????????????????????????????????????
        //     instead of erroring deep in here, we're going to handle it later on
        //      higher up, via rejecting unexpected tokens
        //     actually, wait, we can't do that right? if we want to handle
        //     incomplete "if" statements and such by waiting on the console
        //     how do we deal with that?
        // well actually we can deal with that higher up. if you see the token "if" etc
        // instead of the group
        //we can also add E an error state to RIA for RIAE, E will give us early
        //     warning about broken if clauses and such
        //?????????????????????????????????????????????????
  
        goto fail;
      }
  
      if(drop)
      {
        continue;
      }
  
      if(bundle)
      {
        //Bundle
        SLOP grabbed = wrangle(input_tokens, current_start, travelled, true);
        SLOP bundled = LEXER::token_bundle(mark, -1, -1, grabbed);
        list.append(bundled);
        continue;
      }
  
      // POTENTIAL_OPTIMIZATION_POINT ///////////////////////////
      // uh, this may be unnecessary
      // without it you can skip ahead in bigger increments in parsing
      // i didn't prove that I needed it or didn't need it
      // but you need it if there are any parse patterns who
      // are right substrings of themselves but don't accept the first? maybe.
      // getting rid of it may not speed up any timings at all anyway
      // it won't break anything to have it
      travelled = 1;
      ///////////////////////////////////////////////////////////
  
      //Unbundled: add as is without grouping
      DO(travelled, list.append(input_tokens[current_start + i]);)
      continue;
    }

  succeed:
    return list;
  fail:
    return NIL_UNIT;
  }

  SLOP flat_pass(SLOP &list, I scoop_type)
  {
    // -----------------
    // RE-MARK LAMBDA ARGS      -dependencies: almost everything? probably negate. VERBAL would capture
    // NEGATE NUMBERS           -dependencies: before drop spacing, before ALIKE
    // -----------------
    // TOKEN_GROUP_ALIKE        -dependencies: after negate numbers, hacked to work before drop spaces
    // TOKEN_GROUP_ASSIGNMENT   -dependencies: before SQUARE; so hacked to work before DROP SPACES
    // -----------------
    // TOKEN_GROUP_BOUND_SQUARE -dependencies: after alike, after assign, before drop spacing
    // TOKEN_GROUP_BOUND_ROUND  -dependencies: before drop spacing
    // ----------------- 
    // DROP WHITESPACE
    // -----------------
    // TOKEN_GROUP_CONTROL      -dependencies: after drop spacing
    // TOKEN_GROUP_VERBAL       -dependencies: after alike, bound, drop spacing
    // -----------------
  
    I batch_alike[]           = {TOKENS_NUMBER, TOKENS_ABS_DATETIME, 0};
    I batch_assignment[]      = {TOKEN_GROUP_ASSIGNMENT, 0};
    I batch_bound_round[]     = {TOKEN_GROUP_BOUND_ROUND, 0};
    I batch_bound_square[]    = {TOKEN_GROUP_BOUND_SQUARE, 0};
    I batch_reject_num_var[]  = {TOKEN_GROUP_REJECT_NUM_VAR, 0};
    I batch_control[]         = {TOKEN_GROUP_CONTROL, TOKENS_DEF, TOKENS_IF, 0};
    I batch_verbiage[]        = {TOKEN_GROUP_VERBAL_NNA, TOKEN_GROUP_VERBAL_NVA, TOKEN_GROUP_VERBAL_NA, TOKEN_GROUP_VERBAL_VA, 0};
  
   
    ///////////////////////////////////////////////////
  
    if(TOKEN_GROUP_CURLY_BRACE == scoop_type)
    {
      if(list.count() > 0)
      {
        SLOP first = list[0];
        SLOP v = first;
        I kind = (I)v[+TOKEN_KIND];
        if(TOKEN_GROUP_SQUARE_BRACKET == kind)
        {
          SLOP c = v;
          c.amend_one(+TOKEN_KIND, +TOKEN_GROUP_LAMBDA_ARGS);
          c.amend_one(+TOKEN_KIND_STRING, "lambda args rename");
          list.amend_one(0, c);
        }
      }
    }
  
    list = token_filter_negate_numbers(list);
    list = token_filter_group_dfas(list, batch_reject_num_var);

    list = token_filter_group_dfas(list, batch_alike);
    list = token_filter_group_dfas(list, batch_assignment);

    list = token_filter_group_dfas(list, batch_bound_round);
    list = token_filter_group_dfas(list, batch_bound_square);

    /// DROP WHITESPACE ////
    list = token_filter_drop_whitespace_comments(list);
    ////////////////////////

    list = token_filter_group_dfas(list, batch_control);
    list = token_filter_group_dfas(list, batch_verbiage);

    ///////////////////////////////////////////////////
    
    return list;
  }

  SLOP scoop_separator(SLOP &tokens, I scoop_type, I start, I length, I *middle_ran)
  {
    SLOP bundle = NIL_UNIT;
    SLOP list = UNTYPED_ARRAY;
  
    I p = start;
    I end = start + length;
  
    //Pass 1: Group parentheses, brackets, braces (and SQL)
    //If this thing encounters a LEFT_([{ or SQL_START it will
    //call scoop_top with corresponding arg before continuing.
    //The point is to get a "flat" version for Pass 2.
  
    while(p < end)
    {
      SLOP peek = tokens[+p];
      I kind    = (I)peek[+TOKEN_KIND];
      SLOP payload = peek[+TOKEN_PAYLOAD];
  
      I ran = 0;
  
      I break_flag = false;

      char ascii = '\0';
      char* left = nullptr;
      I index = 0;
      I types[] = {TOKEN_GROUP_ROUND_PAREN, TOKEN_GROUP_SQUARE_BRACKET, TOKEN_GROUP_CURLY_BRACE};
      I top_type = 0;
      SLOP scooped = NIL_UNIT;
  
      switch(kind)
      {
        case TOKENS_SQL_START:
          if(TOKEN_GROUP_SQL == scoop_type)
          {
            //if(p == start)
            //{
              list.append(tokens[p]);
              ran = 1;
            //}
          }
          else
          {
            scooped = scoop_top(tokens, TOKEN_GROUP_SQL, p, end - p, &ran); 
            if(scooped.nullish()) goto fail;
            list.append(scooped);
          }
          break;
        case TOKENS_SQL_MIDDLE:
          if(p == start)
          {
            list.append(tokens[p]);
            ran = 1;
          }
          else
          {
            break_flag = true;
            break;
          }
          break;
        case TOKENS_LEFT:
          ascii = (C)payload[0];
  
          left = (char*)lex_classes['g'];
          assert(!strcmp(left, "([{"));
  
          index = strchr(left, ascii) - left;
          assert(index < strlen(left));
  
          top_type = types[index];
  
          scooped = scoop_top(tokens, top_type, p, end - p, &ran); 
          if(scooped.nullish()) goto fail;
          list.append(scooped);
          break;
        case TOKENS_RIGHT: [[fallthrough]];
        case TOKENS_SEPARATOR: break_flag = true; break;
        default:
        {
          list.append(tokens[p]);
          ran = 1;
          break;
        }
      }
  
      p += ran;
  
      if(break_flag) break;
    }
  
    //Pass 2: With everything grouped (flat), resolve as DFA thingies
    //        This lets us avoid descending into a built tree later
    //        We can modify the flat list without any overhead.
    list = flat_pass(list, scoop_type);
  
    if(list.nullish()) goto fail;
  
  no_mo:
    *middle_ran = p - start; 
  
    bundle = LEXER::token_bundle(TOKEN_GROUP_SEPARATION, -1, -1, list);
  
  succeed:
    return bundle;
  fail:
    return NIL_UNIT;
  }

  SLOP scoop_top(SLOP &tokens, I top_type, I start, I length, I *top_ran)
  {
    //Indirect/Mutually recursive function pair, with scoop_separator
    //scoop_top -> scoop_separator -> scoop_top -> ...
    //Like a recursive descent parser.
    //While the lexer is pretty close to ideal, I have no idea
    //how close this is to ideal. I can see a fine solution
    //and don't have time to keep looking. 2014.12.10
  
    SLOP list(UNTYPED_ARRAY);
  
    I p = start;
    I end = start + length;
  
    I bstart = -1;
    I bend   = -1;

    SLOP bundle = NIL_UNIT;
  
    while(p < end)
    {
      SLOP peek    = tokens[p];
      I kind       = (I)peek[+TOKEN_KIND];
      SLOP payload = (I)peek[+TOKEN_PAYLOAD];
  
      I continue_flag = false;
      I break_flag = false;
  
      switch(top_type)
      {
        case TOKEN_GROUP_SQL: {
          switch(kind)
          {
            case TOKENS_RIGHT: break_flag = true; break;
          }
          break;
        }
        case TOKEN_GROUP_CURLY_BRACE: [[fallthrough]];
        case TOKEN_GROUP_SQUARE_BRACKET: [[fallthrough]];
        case TOKEN_GROUP_ROUND_PAREN:{
          switch(kind)
          {
            case TOKENS_LEFT: {
              if(start == p)
              {
                bstart = (I)peek[+TOKEN_SNIPPET_START];
                p += 1;
                continue_flag = true;
              }
              break;
            }
            case TOKENS_RIGHT: {
                bend = (I)peek[+TOKEN_SNIPPET_END];
                p += 1; 
                break_flag = true;
                break;
            }
          }
          break;
        }
      }
  
      if(TOKENS_SEPARATOR == kind)
      {
        //SQL groups end at the ';' separator, others continue [1;2] (3;4;)
        if(TOKEN_GROUP_SQL == top_type && ';'==payload[0])
        {
          break_flag = true;
        }
        else
        {
          p += 1;
          //continue_flag = true; //not commenting this suppresses empty separations?
        }
      }
  
      if(continue_flag) continue;
      if(break_flag) break;
  
      I scooped_ran = 0;
  
      SLOP scooped = scoop_separator(tokens, top_type, p, end - p, &scooped_ran); 
      
      if(scooped.nullish()) goto fail;
      
      list.append(scooped);
  
      p += scooped_ran;
  
    }
  
    *top_ran = p - start;
    bundle = LEXER::token_bundle(top_type, bstart, bend, list);
  succeed:
    return bundle;
  fail:
    return NIL_UNIT;
  }

  auto parse(SLOP &text, SLOP &tokens)
  {
    reset_lex_parse_globals();

    //If you eliminate this check then you need to track vs.
    //PARSE_MAX_DEPTH in the recursive scooping functions
    I check = check_parenthetical_groupings(tokens);
    I ran = 0;
    SLOP scooped(NIL_UNIT);

    if(check)
    {
      goto fail;
    }

    scooped = scoop_top(tokens, TOKEN_GROUP_PLAIN, 0, tokens.countI(), &ran);
    if(scooped.nullish()) goto fail;
   
  succeed:
    return scooped;
  
  fail:
    has_error = true;
    // K error = NULL;
    // TODO error = new_error_map_lexing_parsing(parse_errno, parse_errno_location, text);
    // reset_lex_parse_globals();
    // return error;
    return scooped;
  }

}; // PARSER


} // namespace
