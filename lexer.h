#pragma once

namespace KERF_NAMESPACE
{

#define MAX_DFA_STATES (13)
#define MAX_DFA_CLASS_SIZE (65)

std::map<std::string, I> The_Reserved;

typedef struct DFA_s{
                     char bundle_mark;
                     const char* ascii_classes;
                     const char* state_names;
                     const char* state_kinds;//Accept-continue, accept-Break, Incomplete/In-progress, Ready/Reject
                     const char* init_string;
                     V grid;
                     char bundle_drop;
}DFA;


//Parts of a token
enum TOKEN_MEMBERS {
                    TOKEN_KIND,
                    TOKEN_KIND_STRING, //optional
                    TOKEN_SNIPPET_START,
                    TOKEN_SNIPPET_END,
                    TOKEN_PAYLOAD,
                    TOKEN_SIZE};

//Kinds of tokens
enum TOKENS_KINDS {TOKENS_NONE=0,

                   TOKENS_COLON,//COLON needs to precede VERB
                   TOKENS_VERB_SYM,//VERB needs to precede NUMBER b/c of '-', NAME b/c of '.'
                   TOKENS_VERB_WORD,//VERB needs to precede NUMBER b/c of '-', NAME b/c of '.'
                   TOKENS_ADVERB_SYM,
                   TOKENS_ADVERB_WORD,
                   TOKENS_NUMBER,//NUMBER must precede NAME to capture ".1"
                   TOKENS_NAME,
                   TOKENS_BYTES,
                   TOKENS_SEPARATOR,//comma,semicolon,newline in/out of [()]
                   TOKENS_ABS_DATE,
                   TOKENS_ABS_DATE_ALT,
                   TOKENS_ABS_TIME,
                   TOKENS_ABS_DATETIME,
                   TOKENS_ABS_DATETIME_ALT,
                   TOKENS_REL_DATETIME,
                   TOKENS_STRING,
                   TOKENS_SINGLE_QUOTE,
                   TOKENS_WHITESPACE,
                   TOKENS_COMMENT,
                   TOKENS_BACKTICK,
                   TOKENS_BACKSLASH,
                   TOKENS_BIGGER,
                   TOKENS_LEFT,
                   TOKENS_RIGHT,
                   TOKENS_SQL_START,
                   TOKENS_SQL_MIDDLE,
                   TOKENS_RESERVED,
                   TOKENS_SELF,
                   TOKENS_RETURN,
                   TOKENS_DEF,
                   TOKENS_WHILE,
                   TOKENS_DO,
                   TOKENS_FOR,
                   TOKENS_IF,
                   TOKENS_ELSE,

                   TOKENS_SIZE};

enum TOKEN_GROUP_KINDS {TOKEN_GROUP_START=TOKENS_SIZE,

                        //Can do in advance
                        TOKEN_GROUP_ALIKE,//vectors: ints, floats, stamps, ...

                        //Must happen at same time because recursive
                        TOKEN_GROUP_PLAIN,
                        TOKEN_GROUP_SQL,
                        TOKEN_GROUP_CURLY_BRACE,
                        TOKEN_GROUP_SQUARE_BRACKET,
                        TOKEN_GROUP_ROUND_PAREN,
                        TOKEN_GROUP_SEPARATION,

                        TOKEN_GROUP_LAMBDA_ARGS,
                        TOKEN_GROUP_ASSIGNMENT,
                        TOKEN_GROUP_BOUND_SQUARE,
                        TOKEN_GROUP_BOUND_ROUND,
                        TOKEN_GROUP_VERBAL_NNA,
                        TOKEN_GROUP_VERBAL_NVA,
                        TOKEN_GROUP_VERBAL_NA,
                        TOKEN_GROUP_VERBAL_VA,
                        TOKEN_GROUP_CONTROL,

                        TOKEN_GROUP_REJECT_NUM_VAR,

                        TOKEN_GROUP_SIZE};


//const char* RESERVED_CONTROL_NAMES[] = {"return", "def", "function", "if", "do", "while", "for", 0};
const char* RESERVED_NUMBER_NAMES[]  = {"inf", "nan", "infinity", 0};
const char* RESERVED_NAME_NAMES[]    = {"nil", "null", "root", "true", "false", 0};
const char* RESERVED_SQL_STARTS[]    = {"select", "update", "insert", "upsert", "delete", 0};
const char* RESERVED_SQL_MIDDLES[]   = {"from", "group", "where", "order", "limit", "values", "set", "on", 0};//TODO: allow as map key, etc, eg {limit:1, set:22}
//const char* RESERVED_SQL_OTHER[]      = {"as", "into", "by", 0};

const char* token_names[] = 
{
  [TOKENS_NONE]         = "none",
  [TOKENS_VERB_SYM]     = "verb_sym",
  [TOKENS_VERB_WORD]    = "verb_word",
  [TOKENS_ADVERB_SYM]   = "adverb_sym",
  [TOKENS_ADVERB_WORD]  = "adverb_word",
  [TOKENS_NAME]         = "name",
  [TOKENS_NUMBER]       = "number",
  [TOKENS_BYTES]        = "bytes",
  [TOKENS_SEPARATOR]    = "separator",
  [TOKENS_ABS_DATE]     = "abs_date",
  [TOKENS_ABS_TIME]     = "abs_time",
  [TOKENS_ABS_DATETIME] = "abs_datetime",
  [TOKENS_REL_DATETIME] = "rel_datetime",
  [TOKENS_STRING]       = "string",
  [TOKENS_SINGLE_QUOTE] = "single_quote",
  [TOKENS_COMMENT]      = "comment",
  [TOKENS_WHITESPACE]   = "whitespace",
  [TOKENS_BACKTICK]     = "backtick",
  [TOKENS_BACKSLASH]    = "backslash",
  [TOKENS_LEFT]         = "left",
  [TOKENS_RIGHT]        = "right",
  [TOKENS_BIGGER]       = "bigger",

  [TOKENS_RESERVED]     = "reserved",

  [TOKENS_DEF]          = "ctrl_def",
  [TOKENS_IF]           = "ctrl_if",
  [TOKENS_DO]           = "ctrl_do",
  [TOKENS_WHILE]        = "ctrl_while",
  [TOKENS_FOR]          = "ctrl_for",
  [TOKENS_ELSE]         = "ctrl_else",
  [TOKENS_RETURN]       = "ctrl_return",
  [TOKENS_COLON]        = "colon",

  [TOKENS_SQL_START]    = "sql_start",
  [TOKENS_SQL_MIDDLE]   = "sql_middle",

  [TOKEN_GROUP_ALIKE]           = "group_alike",

  [TOKEN_GROUP_SQL]             = "group_sql",
  [TOKEN_GROUP_PLAIN]           = "group_plain",
  [TOKEN_GROUP_CURLY_BRACE]     = "group_curly",
  [TOKEN_GROUP_SQUARE_BRACKET]  = "group_square",
  [TOKEN_GROUP_ROUND_PAREN]     = "group_round",
  [TOKEN_GROUP_SEPARATION]      = "group_separation",

  [TOKEN_GROUP_LAMBDA_ARGS]     = "group_lambda_args",
  [TOKEN_GROUP_ASSIGNMENT]      = "group_assignment",
  [TOKEN_GROUP_BOUND_SQUARE]    = "group_bound_square",
  [TOKEN_GROUP_BOUND_ROUND]     = "group_bound_round",
  [TOKEN_GROUP_VERBAL_NNA]      = "group_verbal_nna",
  [TOKEN_GROUP_VERBAL_NVA]      = "group_verbal_nva",
  [TOKEN_GROUP_VERBAL_NA]       = "group_verbal_na",
  [TOKEN_GROUP_VERBAL_VA]       = "group_verbal_va",
  [TOKEN_GROUP_CONTROL]         = "group_control",

  [TOKEN_GROUP_REJECT_NUM_VAR]  = "reject_number_variable", ///eg 0a or 0_a
  
  [TOKEN_GROUP_SIZE] = nullptr,
};


char lex_transition_grids[TOKENS_SIZE][MAX_DFA_STATES][256];
char parse_transition_grids[TOKEN_GROUP_SIZE][MAX_DFA_STATES][256];

const char lex_classes[][MAX_DFA_CLASS_SIZE] = 
{
  ['v']  = "~!@#$%^&*_-+=:|<>.?/\\",
  ['A']  = "/\\=<>~", //add '1' if you want to deal with that & force monadic
  ['a']  = "$._ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
  ['b']  = "._ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
  ['d']  = "0123456789",
  ['e']  = "eE",
  ['.']  = ".",
  ['-']  = "-",
  ['+']  = "+",
  ['0']  = "0",
  ['x']  = "xX",
  ['h']  = "0123456789ABCDEFabcdef",
  ['s']  = ",;\n", //separators
  [':']  = ":",
  ['T']  = "T",
  ['r']  = "YMDHISymdhis",
  ['"']  = "\"", //double quote
  ['\\'] = "\\", //backslash
  ['u']  = "u",
  ['\''] = "'",  //single quote
  ['/']  = "/",
  ['\n'] = "\n", //newline
  ['w']  = " \x20\t\v\f\r", //whitespace; \x20 is a dupe of ' ' for redundancy
  ['`']  = "`",
  ['R']  = "'\"\n",//stops backtick- Reject
  ['g']  = "([{",
  ['G']  = ")]}",
  ['=']  = "=",
  ['<']  = "<",
  ['>']  = ">",
  ['!']  = "!",
  ['o']  = '*',
  //Special
  ['*']  = "*",  //special that handles all ascii chars,
};

DFA LEX_DFAS[] =
{
  //Order here doesn't matter. Order in enum in the .h matters
  //Process ascii classes in reverse order: earlier appearing take precedence in ties
  //Repeating the enum has certain nice properties, among others: repeat mappings
  //Tie-breaking inter-DFA is based on order in TOKENS_MEMBERS
  [TOKENS_COLON]        = {TOKENS_COLON, ":", "01", "RA", "10"},
  [TOKENS_VERB_SYM]     = {TOKENS_VERB_SYM, "v", "01", "RA", "10"},

  [TOKENS_NUMBER]       = {TOKENS_NUMBER, "de.-+", "0123456", "RIIIAAA", 
                                          "40100"
                                          "50000"
                                          "60000"
                                          "60022"
                                          "43500"
                                          "53000"
                                          "60000"},

  [TOKENS_NAME]         = {TOKENS_NAME, "ab", "01", "RA", 
                                        "10"
                                        "11"},


//Intentionally disabled for now to save time.
//To turn on you'll need to complete a parse_bytes path mirroring parse_number
//Use strtoll on pairs but beware ragged half-pair 0x123
//ETA: <30m 
//  [TOKENS_BYTES]        = {TOKENS_BYTES, "0xh", "0123", "RIIA", 
//                                         "100"
//                                         "020"    
//                                         "303"    
//                                         "303"},

  [TOKENS_SEPARATOR]    = {TOKENS_SEPARATOR, "s", "01", "RA", "10"},

  [TOKENS_ABS_DATETIME] = {TOKENS_ABS_DATETIME, "d.-T:", "0123456789ABC", "RHHHHHIIIAAAA", 
                                                "10000"
                                                "12200"
                                                "30000"
                                                "34400"
                                                "50000"
                                                "50060"
                                                "70000" 
                                                "70008" 
                                                "90000" 
                                                "9000A" 
                                                "B0000" 
                                                "BC000" 
                                                "C0000"},
 
  [TOKENS_ABS_DATE]     = {TOKENS_ABS_DATE, "d.-", "012345", "RRRRIA", 
                                            "100"
                                            "120"
                                            "300"
                                            "340"
                                            "500"
                                            "500"},

  //Arguably we could reject "1:1:" and "1:1:1."
  [TOKENS_ABS_TIME]     = {TOKENS_ABS_TIME, "d:.", "0123456", "RIIAAAA",
                                            "100"
                                            "120"
                                            "300"
                                            "340"
                                            "500"
                                            "506"
                                            "600"},


  [TOKENS_REL_DATETIME] = {TOKENS_REL_DATETIME, "dr.-", "0123", "RIIA", 
                                                "1000"
                                                "1300"
                                                "1000"
                                                "1022"},

#if DATES_ALLOW_DASHED
  [TOKENS_ABS_DATE_ALT]     = {TOKENS_ABS_DATE, "d-", "012345", "RHHHIA", 
                                                "10"
                                                "12"
                                                "30"
                                                "34"
                                                "50"
                                                "50"},
//                                                                       2016-02-03
//  [TOKENS_ABS_DATE_ALT]     = {TOKENS_ABS_DATE, "d-", "0123456789A", "RRRRHHHHHIA", 
//                                                "10"
//                                                "20"
//                                                "30"
//                                                "40"
//                                                "05"
//                                                "60"
//                                                "70"
//                                                "08"
//                                                "90"
//                                                "A0"
//                                                "00"},
 
#else
  [TOKENS_ABS_DATE_ALT]     = {0},
#endif
 
   
  //Double and single quoted strings. We could allow \uX\b and such if we want
  [TOKENS_STRING]       = {TOKENS_STRING, "\"\\uh*", "01234567", "RIIIIIIA", "10000"
                                                                             "72111"
                                                                             "11311"
                                                                             "02141"
                                                                             "02151"
                                                                             "02161"
                                                                             "02111"
                                                                             "10000"},

  [TOKENS_SINGLE_QUOTE] = {TOKENS_STRING, "'\\uh*", "01234567", "RIIIIIIA",  "10000"
                                                                             "72111"
                                                                             "11311"
                                                                             "02141"
                                                                             "02151"
                                                                             "02161"
                                                                             "02111"
                                                                             "10000"},
  [TOKENS_WHITESPACE]   = {TOKENS_WHITESPACE, "w", "01", "RA", "11"},

  [TOKENS_COMMENT]      = {TOKENS_COMMENT, "/\n*", "012", "RIA", "100"
                                                                 "200"
                                                                 "202"},

  [TOKENS_BACKTICK]     = {TOKENS_BACKTICK, "R`bv", "012", "RIA", 
                                            "0100"
                                            "0222"
                                            "0000"},

  [TOKENS_ADVERB_SYM]   = {TOKENS_ADVERB_SYM, "\\A", "012", "RIA",
                                               "10"
                                               "22"
                                               "00"},

  [TOKENS_BACKSLASH]    = {TOKENS_BACKSLASH, "\\A*", "012", "RIR", //off. "RIA" to turn on
                                              "120"
                                              "222"
                                              "020"},

  [TOKENS_BIGGER]       = {TOKENS_VERB_SYM, "=><!o", "01234", "RIIIA", 
                                            "11213"
                                            "40000"    
                                            "44000"    
                                            "00004"
                                            "00000"},

  [TOKENS_LEFT]         = {TOKENS_LEFT,  "g", "01", "RA", "10"},
  [TOKENS_RIGHT]        = {TOKENS_RIGHT, "G", "01", "RA", "10"},

  [TOKENS_SIZE]         = {0}, //required if some enum members have no DFA
};

#pragma mark - Parser DFAs and Classes

char parse_classes[][MAX_DFA_CLASS_SIZE] =
{
  ['A'] = {TOKENS_NAME, TOKENS_STRING, 0},
  ['N'] = {TOKENS_NAME,0},
  ['0'] = {TOKENS_NUMBER,0},
  ['d'] = {TOKENS_ABS_DATE, TOKENS_ABS_TIME, TOKENS_ABS_DATETIME,0},
  ['s'] = {TOKENS_WHITESPACE, TOKENS_COMMENT, 0},

  ['C'] = {TOKENS_DO, TOKENS_WHILE, TOKENS_FOR,0},
  ['D'] = {TOKENS_DEF,0},
  ['I'] = {TOKENS_IF,0},
  ['E'] = {TOKENS_ELSE,0},
  
  ['('] = {TOKEN_GROUP_ROUND_PAREN,0},
  ['['] = {TOKEN_GROUP_SQUARE_BRACKET,0},
  ['{'] = {TOKEN_GROUP_CURLY_BRACE,0},
  ['v'] = {TOKENS_VERB_SYM, TOKENS_VERB_WORD,0},
  ['a'] = {TOKENS_ADVERB_SYM, TOKENS_ADVERB_WORD,0},
  ['b'] = {TOKEN_GROUP_BOUND_ROUND,0},
  ['w'] = {TOKENS_VERB_WORD,0},
  ['W'] = {TOKENS_ADVERB_WORD,0},
  [':'] = {TOKENS_COLON,0},
  ['p'] = {TOKENS_NAME, TOKENS_VERB_WORD, TOKEN_GROUP_CURLY_BRACE, TOKENS_SELF, 0},  //p( binds. Adverbs disallowed for now: see notes.

  //^ whitespace comment adverb verb_sym verb_word ?  might be smarter
  ['n'] = {TOKENS_NUMBER,TOKENS_NAME,TOKENS_BYTES, TOKENS_ABS_DATE, TOKENS_ABS_TIME, TOKENS_ABS_DATETIME,
           TOKENS_REL_DATETIME, TOKENS_STRING, TOKENS_SINGLE_QUOTE, TOKENS_BACKTICK, TOKENS_BACKSLASH, TOKENS_BIGGER,
           TOKENS_RESERVED, TOKEN_GROUP_ALIKE, TOKEN_GROUP_SQL, TOKEN_GROUP_PLAIN, TOKEN_GROUP_CURLY_BRACE,
           TOKEN_GROUP_SQUARE_BRACKET, TOKEN_GROUP_ROUND_PAREN, TOKEN_GROUP_BOUND_SQUARE, TOKEN_GROUP_BOUND_ROUND,
           TOKEN_GROUP_ALIKE, TOKEN_GROUP_CONTROL, 0},

  //Special
  ['*'] = {0},//special ALL
};

DFA PARSE_DFAS[] =
{
  [TOKENS_NUMBER]       = {TOKEN_GROUP_ALIKE, "0s", "01", "RA", 
                                              "10"
                                              "11"},

  [TOKENS_ABS_DATETIME] = {TOKEN_GROUP_ALIKE, "ds", "01", "RA", 
                                              "10"
                                              "11"},

  [TOKEN_GROUP_ASSIGNMENT] = {TOKEN_GROUP_ASSIGNMENT, "A[sv:", "01234", "RIIIA", 
                                                      "10000"
                                                      "01234"
                                                      "00234"
                                                      "00004"
                                                      "00000"}, 

  [TOKEN_GROUP_BOUND_ROUND] = {TOKEN_GROUP_BOUND_ROUND, "p(", "012", "RIA", 
                                                        "10"
                                                        "02"
                                                        "00"},

  [TOKEN_GROUP_BOUND_SQUARE] = {TOKEN_GROUP_BOUND_SQUARE, "[n", "012", "RIA",  //backwards because [ ⊂ n
                                                          "11"
                                                          "20" //n[][] same as n[;] in fwd order
                                                          "20"},

  //DO WHILE FOR
  [TOKEN_GROUP_CONTROL] = {TOKEN_GROUP_CONTROL, "C({", "0123", "REEA", 
                                                "100"
                                                "020"
                                                "003"
                                                "000"},

//  [TOKENS_DEF] = {TOKEN_GROUP_CONTROL, "DN({", "01234", "REEEA", 
//                                       "1000"
//                                       "0200"
//                                       "0030"
//                                       "0004"
//                                       "0000"},

  [TOKENS_DEF] = {TOKEN_GROUP_CONTROL, "Db{", "0123", "REEA", //'b' b/c Name gets captured w/ "()" as bound round
                                       "100"
                                       "020"
                                       "003"
                                       "000"},

  [TOKENS_IF] = {TOKEN_GROUP_CONTROL, "I({E", "012345", "REEAEA", //  I({[EI({]*[E{]
                                      "1000"
                                      "0200"
                                      "0030"
                                      "0004"
                                      "1050"
                                      "0000"},

//If munged, we just have to separate it again later, and the separation is complicated
//  [TOKEN_GROUP_VERBAL] = {TOKEN_GROUP_VERBAL, "nva", "0123", "RIIA", // X: [[n]na+ | [n]va*]
//                                              "130"
//                                              "233"
//                                              "003"
//                                              "003"},

  [TOKEN_GROUP_VERBAL_NNA] = {TOKEN_GROUP_VERBAL_NNA, "na", "0123", "RRRA", //nna+
                                                      "10"
                                                      "20"
                                                      "03"
                                                      "03"},

  [TOKEN_GROUP_VERBAL_NVA] = {TOKEN_GROUP_VERBAL_NVA, "nva", "012", "RRA", //nva*
                                                      "100"
                                                      "020"
                                                      "002"},

  [TOKEN_GROUP_VERBAL_NA] = {TOKEN_GROUP_VERBAL_NA, "na", "012", "RRA", //na+
                                                    "10"
                                                    "02"
                                                    "02"},

  [TOKEN_GROUP_VERBAL_VA] = {TOKEN_GROUP_VERBAL_VA, "va", "01", "RA", //va*
                                                    "10"
                                                    "01"},

  [TOKEN_GROUP_REJECT_NUM_VAR] = {TOKEN_GROUP_REJECT_NUM_VAR, "0N", "012", "RRE", 
                                                              "10"
                                                              "02"
                                                              "22"},

  [TOKEN_GROUP_SIZE]         = {0}, //required if some enum members have no DFA
};

#pragma mark -

void dfa_populate(DFA dfa_array[], I array_len, const char class_maps[][MAX_DFA_CLASS_SIZE])
{
  //Initialize the transition grids
  //We expand the shorthand used in the DFA structs to real transition matrices
  //of width 256 (all ascii/char values), one row for each state, cells are
  //non-negative integers of type char corresponding to a state.
  DO(array_len, DFA dfa = dfa_array[i]; 
                I token_kind  = i;
                      char* grid    = (char*)dfa.grid;
                const char* states  = dfa.state_names;
                const char* classes = dfa.ascii_classes;
                const char* chart   = dfa.init_string;

                if(!states || !classes) continue;//unitialized DFA in array
                I N_states  = strlen(states);
                I N_classes = strlen(classes);

                assert(strlen(chart) == N_states * N_classes);
                assert(strlen(dfa.state_kinds) == N_states);

                DO(N_states,
                  DO2(N_classes, I back = (N_classes - 1) - j;//step backwards
                                 C aclass = classes[back];
                                 C destination = chart[i*N_classes + back];
                                 UC state_index = strchr(states, destination) - states;

                                 assert(state_index >= 0);
                                 assert(state_index < MAX_DFA_STATES);

                                 const char* map = class_maps[aclass];
                                 I N_map = strlen(map);
                                 if('*' == aclass)//special: all
                                   DO3(256,   grid[i*256 +     k ] = state_index)
                                 else 
                                   DO3(N_map, grid[i*256 + map[k]] = state_index;)
                  )

                //DO3(127, O("%d", grid[i*256 + k]))O("\n"); //print
                )
                //O("\n");
  )
}

void dfa_init()
{
  I max_states = 0;
  I max_class_size = 0;

  DO(ARRAY_LEN(  LEX_DFAS), const char* s =   LEX_DFAS[i].state_names; if(s) max_states = MAX(max_states, strlen(s)))
  DO(ARRAY_LEN(PARSE_DFAS), const char* s = PARSE_DFAS[i].state_names; if(s) max_states = MAX(max_states, strlen(s)))
  assert(max_states == MAX_DFA_STATES);//enforce grids not ridiculously large in .bss

  DO(ARRAY_LEN(lex_classes),   const char* s =   lex_classes[i]; if(s) max_class_size = MAX(max_class_size, strlen(s)))
  DO(ARRAY_LEN(parse_classes), const char* s = parse_classes[i]; if(s) max_class_size = MAX(max_class_size, strlen(s)))
  assert(max_class_size+1 == MAX_DFA_CLASS_SIZE); // enforce grids not ridiculously large in .bss

  assert(TOKEN_GROUP_SIZE <= 256); //so the parser can use the 256 char thing from the lexer

  //For each token class, assign it the space for its transition grid (.bss again)
  DO(ARRAY_LEN(LEX_DFAS), LEX_DFAS[i].grid = &lex_transition_grids[i])
  DO(ARRAY_LEN(PARSE_DFAS), PARSE_DFAS[i].grid = &parse_transition_grids[i])

  dfa_populate(LEX_DFAS, ARRAY_LEN(LEX_DFAS), lex_classes);
  dfa_populate(PARSE_DFAS, ARRAY_LEN(PARSE_DFAS), parse_classes);
}

void reserved_init()
{

  char* *s = nullptr;

  // for(s=(char**)RESERVED_CONTROL_NAMES; *s; s++) The_Reserved[*s] = TOKENS_CONTROL;
  for(s=(char**)RESERVED_NUMBER_NAMES;  *s; s++) The_Reserved[*s] = TOKENS_NUMBER;
  for(s=(char**)RESERVED_NAME_NAMES;    *s; s++) The_Reserved[*s] = TOKENS_RESERVED;
  for(s=(char**)RESERVED_SQL_STARTS;    *s; s++) The_Reserved[*s] = TOKENS_SQL_START;
  for(s=(char**)RESERVED_SQL_MIDDLES;   *s; s++) The_Reserved[*s] = TOKENS_SQL_MIDDLE;

  The_Reserved["self"]     = TOKENS_SELF;
  The_Reserved["this"]     = TOKENS_SELF;
  The_Reserved["def"]      = TOKENS_DEF;
  The_Reserved["function"] = TOKENS_DEF;
  The_Reserved["if"]       = TOKENS_IF;
  The_Reserved["do"]       = TOKENS_DO;
  The_Reserved["while"]    = TOKENS_WHILE;
  The_Reserved["for"]      = TOKENS_FOR;
  The_Reserved["else"]     = TOKENS_ELSE;
  The_Reserved["return"]   = TOKENS_RETURN;

  // TODO reserved_verbs_init();
  // TODO reserved_adverbs_init();
}

I reserved_lookup(std::string &x, bool case_insensitive)
{
  std::string y = x;

  if(case_insensitive)
  {
    std::transform(y.begin(), y.end(), y.begin(), [](unsigned char c) -> unsigned char { return std::tolower(c); });
  }

  if(The_Reserved.contains(y)) return The_Reserved[y];
  return TOKENS_NONE;
}

#pragma mark - Lexer / Tokenizer

// POP? move these to per-thread data [normalized], as the kvm and such are, instead of __kerfthread
// POP have local versions on the LEXER/PARSER instance and then only set the globals when done
__kerfthread static int lex_errno;
__kerfthread static int lex_errno_location;
__kerfthread static int parse_errno;
__kerfthread static int parse_errno_location;

struct LEXER
{
  bool has_error = false;
  SLOP tokens = UNTYPED_ARRAY;
  SLOP par_stack = CHAR0_ARRAY;
  SLOP text = CHAR0_ARRAY;

  // Feature. Can allow unfinished strings eg lex("\"sdsdsd") and then lex("\n") and then lex("asas\"");
  // Feature. Similarly, for unfinished SQL, if that's even a thing (I don't remember exactly). In this case it's probably [more] OK to do an O(n^2) parser technique, since we expect SQL statements to be much shorter than say long maps/dictionaries of data.

  // T̶O̶D̶O̶. maintain a running stack of open parentheticals and open quotes to detect when a lex/parse unit (such as a script) is complete [in scriptfile-reading and terminal-reading and so on]. see xload POP in kerf1 and is_incomplete. Update: we added this as `parenthetically_complete` so all you have to do is check that and keep `lex()`ing. There may be some wrinkle with parsing [or not] newlines, it shouldn't be hard to solve. Idea When testing incompleteness you can use two lexers and it's fine. one to test for incompleteness and one to run on the completed string, if necessary. Alternatively. The one lexer can track everything on its own 
  bool parenthetically_complete()
  {
    // Does not mean error free. Merely that we don't still need to read,
    // for the purposes of efficiently parsing scripts in O(n) time not O(n^2) time
    // when parsing long dictionaries and such
    return par_stack.count() == 0;
  }
  bool is_complete()
  {
    return parenthetically_complete() && true && true;
  }
  bool is_incomplete() {return !is_complete();}

  static void reset_lex_parse_globals()
  {
    lex_errno = 0;
    lex_errno_location = -1;
  
    parse_errno = 0;
    parse_errno_location = -1;
  }

  static void init()
  {
    // Remark. It's cleaner if we move all the lexing globals into the LEXER class as static members? Gonna be a little ugly cause parser init logic is mixed in 
    reserved_init();
    dfa_init();
    LEXER::reset_lex_parse_globals();
  }

  void test()
  {
    er(LEX TEST)
    lex("(+\\/1 -2222 3");
    lex(") 4");
    lex("1 2+1");
    tokens.show();
    std::cerr << "has_error: " << (has_error) << "\n";
    std::cerr << "par_stack: " << (par_stack) << "\n";
  }

  void reset()
  {
    *this = LEXER(); 
  }

  static SLOP token_bundle(I mark, I start, I end, SLOP& payload)
  {
    SLOP bundle(UNTYPED_ARRAY);
  
    const char* name = token_names[mark];
  
    if(payload.layout()->header_get_presented_type() != STRING_UNIT) // eg, is a non-string array
    {
      I snippet_start = 0;
      I snippet_end = 0;
      I p = payload.countI();
  
      if(p > 0)
      {
        SLOP first = payload[0];
        SLOP last = payload[p-1];
        snippet_start = I(first[+TOKEN_SNIPPET_START]);
        snippet_end = I(last[+TOKEN_SNIPPET_END]);
      }
  
      if(-1==start) start = snippet_start;
      if(-1==end)   end   = snippet_end;
    }
  
    bundle.append(mark);    // TOKEN_KIND         
    bundle.append(name);    // TOKEN_KIND_STRING  
    bundle.append(start);   // TOKEN_SNIPPET_START
    bundle.append(end);     // TOKEN_SNIPPET_END  
    bundle.append(payload); // TOKEN_PAYLOAD      

    return bundle;
  }

  SLOP toke(SLOP &text, I start, I *travelled)
  {
    I position = start;
    *travelled = 0;
  
    C prev_states[TOKENS_SIZE] = {0};
    C      states[TOKENS_SIZE] = {0};
    I run_lengths[TOKENS_SIZE] = {0}; 
  
    I max_run = 0;
    I max_run_index = 0;
    I max_run_state = 0;
  
    I legit_position  = position;
  
    I legit_max       = 0;
    I legit_max_index = 0;
    I legit_max_state = 0;
  
    while(position < text.length())
    {
      C ascii = (C)text[position];
  
      I current_max = 0;
      I current_max_index = 0;
      I current_max_state = 0;
      C current_max_kind  = '\0';
  
      //step
      DO(TOKENS_SIZE, DFA dfa = LEX_DFAS[i]; 
                      C *grid = (char*) dfa.grid;
                      C *state_kinds = (char*) dfa.state_kinds;
                      if(!state_kinds) continue;
                      I before = states[i];
                      prev_states[i] = before;
                      states[i] = grid[256*before + ascii];
                      run_lengths[i]++;
                      C kind = state_kinds[states[i]];
                      if(0==states[i]) run_lengths[i] = 0;
  
                      bool special_capture = false;
  
                      //special case `"..." and `'...'
                      bool backtick_string = (TOKENS_STRING==i || TOKENS_SINGLE_QUOTE==i)
                      && (TOKENS_BACKTICK==max_run_index) && (1==max_run) && (1==run_lengths[i]);
                      if(backtick_string)
                      {
                        special_capture = true;
                      }
  
                      if(special_capture)
                      {
                        run_lengths[i] = 1 + max_run;
                      }
  
                      switch(kind)
                      {
                        case 'R': break;
  
                        case 'H': //H for Hopeful
                        case 'I': 
                        case 'A':
                        default:     
                                if(current_max < run_lengths[i])
                                {
                                  // "<" ensures we take the first token kind in the event of a tie
                                  current_max       = run_lengths[i];
                                  current_max_index = i;
                                  current_max_state = states[i];
                                  current_max_kind  = kind;
                               }
                      }
      )
  
      if(current_max <= max_run)
      {
        break;
      }
  
      //keep stepping
      max_run       = current_max;
      max_run_index = current_max_index;
      max_run_state = current_max_state;
  
      switch(current_max_kind)
      {
        case 'H': break;
        default:
            legit_max       = max_run;
            legit_max_index = max_run_index;
            legit_max_state = max_run_state;
            legit_position  = position;
      }
  
      position++;
    }
  
    max_run       = legit_max;
    max_run_index = legit_max_index;
    max_run_state = legit_max_state;
  
    *travelled = max_run;
  
    if(max_run <= 0)
    {
      lex_errno = ERROR_LEX_UNKNOWN;
      lex_errno_location = position; 
      return NIL_UNIT;
    }
  
    I i     = max_run_index;
    I state = max_run_state;
    C kind  = LEX_DFAS[i].state_kinds[state];
  
    switch(kind)
    {
      case 'I': lex_errno=ERROR_LEX_INCOMPLETE; lex_errno_location = legit_position; return NIL_UNIT;
      case 'R': lex_errno=ERROR_LEX_UNKNOWN;    lex_errno_location = legit_position; return NIL_UNIT;
    }
  
    I token_index = max_run_index;
    I mark = LEX_DFAS[token_index].bundle_mark;
  
    std::string p = "";
    DO(max_run, p.push_back((C)text[start+i]))
  
    I end = start + max_run;

    //Relabel reserved
    if(TOKENS_NAME == mark)
    {
      bool insensitive = PARSE_RESERVED_CASE_INSENSITIVE;    
        
      I reservation = reserved_lookup(p, insensitive);
  
      if(TOKENS_NONE != reservation)
      {
        assert(0 <= reservation);
        assert(reservation < TOKENS_SIZE); 
  
        mark = reservation; 
      }
    }

    assert(0 <= mark);
    assert(mark < TOKENS_SIZE);
  
    SLOP payload(p);

    if(TOKENS_LEFT == mark)
    {
      switch(C(payload[0]))
      {
        case '{': par_stack.append('}'); break;
        case '[': par_stack.append(']'); break;
        case '(': par_stack.append(')'); break;
      }

      if(par_stack.count() > PARSE_MAX_DEPTH)
      {
        // depth exceeded
      }
    }
    else if(TOKENS_RIGHT == mark)
    {
      C c = C(payload[0]);

      if(par_stack.count() < 1)
      {
        // overmatch. currently caught by parser
      }
      else if(par_stack.count() >= 1 && c != par_stack[par_stack.count() - 1])
      {
        // mismatch. currently caught by parser
      }
      else
      {
        // ok
      }

      // Feature. as of this writing we don't have a cow_pop() method
      par_stack.layout()->header_add_n_chunk_width_to_byte_counter_unchecked(-1);
    }

    //Put all the validated token information into a LIST
    SLOP bundle = token_bundle(mark, start, end, payload);
  
    return bundle;
  }

  void lex(std::string &&snippet)
  {
    LEXER::reset_lex_parse_globals();
  
    I current_start = text.countI();

    text.join(snippet);
  
    while(current_start < text.countI())
    {
      I travelled = 0;
  
      SLOP u = toke(text, current_start, &travelled);
  
      if(u.nullish())
      {
        goto fail;
      }
  
      tokens.append(u);
  
      current_start += travelled;
    }
  
    if(lex_errno)
    {
      goto fail;
    }
    
  succeed:
    return;
  
  fail:
    has_error = true;
    // TODO K error = new_error_map_lexing_parsing(lex_errno, lex_errno_location, text);
    // reset_lex_parse_globals();
    // return error;
    return;
  }

}; // LEXER

LEXER The_Console_Lexer;

} // namespace
