# kerf-source


  This is "Kerf" or "Kerf2". It is incomplete. Kerf2 is designed for full concurrency. It also adds transparent type promotion, a C++ instead of C basis, and more.

  The source for the complete "Kerf1" is at https://github.com/kevinlawler/kerf1/tree/master/src


# Contact


    kerf.concerns@gmail.com


# Compile on macOS, ...


    #install google test. should be `brew install googletest`

    clang++ -std=c++2b -pthread -ledit -levent -O0 -DDEBUG -g -lgtest -fsanitize=address -ferror-limit=5 main.cc


# Run


    ./a.out

