#pragma once

namespace KERF_NAMESPACE
{

struct INTERPRETER
{
  static SLOP interpret(SLOP text)
  {
    LEXER l;

    l.lex(text);

    kerr() << "lexer tokens: " << (l.tokens) << "\n";
    kerr() << "lexer.has_error: " << (l.has_error) << "\n";

    PARSER par;
    auto t = par.parse(l.text, l.tokens);
    kerr() << "parse tree: " << (t) << "\n";
    std::cerr <<  "par.has_error: " << (par.has_error) << "\n";

    return NIL_UNIT;
  }
};
 

} // namespace
