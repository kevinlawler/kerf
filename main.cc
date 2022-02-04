#include "kerf.h"

namespace KERF_NAMESPACE {


void kevin_try_stuff()
{
  return;
}


event_base* eb;
event *t;
const char *KERFPROMPT = "kerf> ";
const char *KERFCONTINUEPROMPT = "> ";

static void event_log_callback(int sev, const char* msg)
{
  switch (sev) {
    default:
    case _EVENT_LOG_MSG:   kerr() << "Event Message: " << msg; break;
    case _EVENT_LOG_DEBUG: kerr() << "Event Debug: "   << msg; break;
    case _EVENT_LOG_WARN:  kerr() << "Event Warn: "    << msg; break;
    case _EVENT_LOG_ERR:   kerr() << "Event Error: "   << msg; break;
  }
}

static void event_fatal_callback(int err)
{
  kerr() << "Event Fatal: " << err;
}

void line_handler(char* line) 
{
  rl_completion_append_character = '\0'; //this stops a space from appearing after tab completion

  if (line==NULL) // Ctrl-D
  {
    kout() << "\n"; // next terminal prompt start on clean line
    kerf_exit(0);
  }
  else 
  {
    if(*line!=0) 
    {
      add_history(line);
    }

    if(The_Console_Lexer.text.size() > 0)  The_Console_Lexer.lex("\n");
    The_Console_Lexer.lex(line);
    free(line);

    auto complete = The_Console_Lexer.is_complete();

    if(!complete) goto incomplete;

    SLOP text = The_Console_Lexer.text;
    The_Console_Lexer.reset();

    auto s = INTERPRETER::interpret(text);

  }

complete:
  rl_callback_handler_remove();
  rl_callback_handler_install(KERFPROMPT, &line_handler);
  return;

incomplete:
  rl_callback_handler_remove();
  rl_callback_handler_install(KERFCONTINUEPROMPT, &line_handler);
  return;
}

void event_handler(int fd, short event, void *arg)
{
  if(event & EV_READ)
  {
    rl_callback_read_char();
  }
}

void attend()
{
  kout() << "\n";
  // Reading from stdin may appear laggy in MacVim's "console" but won't be from Terminal.app
  rl_callback_handler_install(KERFPROMPT, &line_handler);

  event_set_log_callback(event_log_callback);
  event_enable_debug_logging(EVENT_DBG_ALL);
  event_set_fatal_callback(event_fatal_callback);

  eb = event_base_new();
  t = event_new(eb, STDIN_FILENO, EV_READ | EV_PERSIST, event_handler, NULL);
  event_add(t, NULL);
  event_base_loop(eb, EVLOOP_NO_EXIT_ON_EMPTY); // EVLOOP_ONCE EVLOOP_NONBLOCK 
}


} // namespace

int main(int argc, char** argv)
{
  extern char **environ; // POSIX. not `char **envp` after `argv`.

  using namespace KERF_NAMESPACE;

  bool reinit = true;
  kerf_init(reinit);

  hard_jmp_wrapper([&] (int val) {
    switch(val)
    {
      case 0:
      {
        The_Can_Jump = true;
    
      // TODO. run test suite 10k times and make sure memory does not grow. view using Activity Monitor. Note. still grows with workstacks disabled. may be malloc free() not releasing? Idea. toggle the workstack, then bisect comment tests (is it the mapcores one?), maybe bisect compiler options
      #if DEBUG
        ::testing::InitGoogleTest(&argc, argv);
        bool fail = false;
        DO(1, fail = RUN_ALL_TESTS(); pop_workstacks_for_normalized_thread_id();)
        if(!fail){} // TODO run with other PREFERRED type ? how to do in context of GoogleTest ? nested call? see `Value-Parameterized Test Parameters` or `Typed Tests` and the like. Could also cheat with --gtest_repeat and increment the PREFERRED.
        if(fail){kerf_exit(1);}
      #endif
   
        DO(1, kevin_try_stuff(); pop_workstacks_for_normalized_thread_id();)
       
        break;
      }
      case ERROR_CTRL_C:
      case 1: [[fallthrough]];
      default:
      {
        //if(! finished initializing)
        // fprintf(stderr, "\nError in initialization. Kerf will exit as a precaution.\n");
        // kerf_exit(EX_OSERR);

        // fprintf(stderr, "\nSiglongjmp inside of main() function.\n");

        SLOP err((ERROR_TYPE)val);
        err.show();

        break;
      }
    }
  
    if(eb)event_base_loopbreak(eb);
    if(t)event_free(t);
    if(eb)event_base_free(eb);
    rl_callback_handler_remove();
    The_Console_Lexer.reset();

    if(CONSOLE) attend();

  });


  return EXIT_SUCCESS;
}

