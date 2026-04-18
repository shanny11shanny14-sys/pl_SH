#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum class TokenType {
  LeftParen,
  RightParen,
  Dot,
  Quote,
  Int,
  Float,
  String,
  Nil,
  True,
  Symbol,
  EndOfFile,
  NoClosingQuote
};

enum class NodeType {
  Nil,
  True,
  Int,
  Float,
  String,
  Symbol,
  Cons,
  Procedure,
  Message
};

enum class ErrorType {
  UnexpectedAtomOrLeftParen,
  UnexpectedRightParen,
  NoClosingQuote,
  NoMoreInput
};

enum class EvalErrorType {
  NonList,
  IncorrectNumberOfArguments,
  IncorrectArgumentType,
  AttemptToApplyNonFunction,
  NoReturnValue,
  UnboundSymbol,
  DivisionByZero,
  DefineFormat,
  CondFormat,
  LevelOfDefine,
  LevelOfCleanEnvironment,
  LevelOfExit
};

struct Token {
  TokenType type;
  string input;
  string output;
  int line;
  int column;
};

struct Node {
  NodeType type;
  string text;
  Node *left;
  Node *right;
};

struct error_message {
  ErrorType type;
  string tokenText;
  int line;
  int column;
};

struct SourcePosition {
  int line;
  int column;
};

struct EvalError {
  EvalErrorType type;
  string name;
  Node *node;
};

using NodePtr = Node*;

NodePtr MakeNode(NodeType type, string text = "") {
  NodePtr node(new Node());
  node->type = type;
  node->text = text;
  node->left = NULL;
  node->right = NULL;
  return node;
}

NodePtr MakeNil() {
  return MakeNode(NodeType::Nil, "nil");
}

NodePtr MakeTrue() {
  return MakeNode(NodeType::True, "#t");
}

NodePtr MakeProcedure(string name) {
  return MakeNode(NodeType::Procedure, name);
}

NodePtr MakeMessage(string text) {
  return MakeNode(NodeType::Message, text);
}

NodePtr MakeCons(NodePtr left, NodePtr right) {
  NodePtr node = MakeNode(NodeType::Cons);
  node->left = left;
  node->right = right;
  return node;
}

NodePtr MakeAtomFromToken(Token token) {
  if (token.type == TokenType::Nil) return MakeNil();
  if (token.type == TokenType::True) return MakeTrue();
  if (token.type == TokenType::Int) return MakeNode(NodeType::Int, token.input);
  if (token.type == TokenType::Float) return MakeNode(NodeType::Float, token.input);
  if (token.type == TokenType::String) return MakeNode(NodeType::String, token.input);
  return MakeNode(NodeType::Symbol, token.input);
}

NodePtr BuildList(vector<NodePtr> items, NodePtr tail) {
  NodePtr result = tail;
  for (int i = (int)items.size() - 1; i >= 0; --i) {
    result = MakeCons(items[i], result);
  }

  return result;
}

bool IsSeparator(char ch) {
  return isspace((unsigned char)ch) || ch == '(' || ch == ')' ||
         ch == '\'' || ch == '"' || ch == ';';
}

bool IsPrintable(char ch) {
  return isprint((unsigned char)ch) != 0;
}

string Trim(string text) {
  int left = 0;
  int right = (int)text.size() - 1;

  while (left <= right && isspace((unsigned char)text[left])) {
    left++;
  }

  while (right >= left && isspace((unsigned char)text[right])) {
    right--;
  }

  if (left > right) return "";
  return text.substr(left, right - left + 1);
}

bool IsIntToken(string text) {
  if (text.empty()) return false;

  int index = 0;
  if (text[index] == '+' || text[index] == '-') {
    index++;
  }

  if (index >= (int)text.size()) return false;

  for (int i = index; i < (int)text.size(); ++i) {
    if (!isdigit((unsigned char)text[i])) {
      return false;
    }
  }

  return true;
}

bool IsFloatToken(string text) {
  if (text.empty()) return false;

  int index = 0;
  if (text[index] == '+' || text[index] == '-') {
    index++;
  }

  if (index >= (int)text.size()) return false;

  int dotCount = 0;
  int digitCount = 0;
  for (int i = index; i < (int)text.size(); ++i) {
    char ch = text[i];
    if (ch == '.') {
      dotCount++;
      if (dotCount > 1) return false;
    } else if (isdigit((unsigned char)ch)) {
      digitCount++;
    } else {
      return false;
    }
  }

  if (dotCount != 1 || digitCount == 0) return false;

  return text != ".";
}

Token ClassifyTokenText(string text, int line, int column) {
  Token token;
  token.input = text;
  token.output = text;
  token.line = line;
  token.column = column;

  if (text == ".") token.type = TokenType::Dot;
  else if (text == "nil" || text == "#f") token.type = TokenType::Nil;
  else if (text == "t" || text == "#t") token.type = TokenType::True;
  else if (IsIntToken(text)) token.type = TokenType::Int;
  else if (IsFloatToken(text)) token.type = TokenType::Float;
  else token.type = TokenType::Symbol;

  return token;
}

class Lexer {
 private:
  string m_line;
  string m_bufferedLine;
  int index_line;
  int position;
  bool else_line;
  bool buffered_line;
  bool end;

  bool ReadNextLine() {
    if (buffered_line) {
      m_line = m_bufferedLine;
      buffered_line = false;
    } else {
      if (!getline(cin, m_line)) {
        else_line = false;
        end = true;
        return false;
      }
    }

    index_line++;
    position = 0;
    else_line = true;
    return true;
  }

  Token MakeSimpleToken(TokenType type, string text, int line, int column) {
    Token token;
    token.type = type;
    token.input = text;
    token.output = text;
    token.line = line;
    token.column = column;
    return token;
  }

 public:
  Lexer(string bufferedLine = "", bool hasBufferedLine = false) {
    m_bufferedLine = bufferedLine;
    index_line = 0;
    position = 0;
    else_line = false;
    buffered_line = hasBufferedLine;
    end = false;
  }

  SourcePosition CurrentPosition() {
    if (else_line) {
      int probe = position;
      while (probe < (int)m_line.size() &&
             isspace((unsigned char)m_line[probe])) {
        probe++;
      }

      if (probe >= (int)m_line.size() || m_line[probe] == ';') {
        return {index_line + 1, 1};
      }

      return {index_line, position + 1};
    }

    return {index_line + 1, 1};
  }

  Token NextToken() {
    while (true) {
      if ((!else_line || position >= (int)m_line.size()) && !ReadNextLine()) {
        return MakeSimpleToken(TokenType::EndOfFile, "", index_line + 1, 1);
      }

      while (position < (int)m_line.size() &&
             isspace((unsigned char)m_line[position])) {
        position++;
      }

      if (position >= (int)m_line.size()) {
        continue;
      }

      if (m_line[position] == ';') {
        position = (int)m_line.size();
        continue;
      }

      break;
    }

    int startLine = index_line;
    int startColumn = position + 1;
    char ch = m_line[position];

    if (ch == '(') {
      position++;
      return MakeSimpleToken(TokenType::LeftParen, "(", startLine, startColumn);
    }

    if (ch == ')') {
      position++;
      return MakeSimpleToken(TokenType::RightParen, ")", startLine, startColumn);
    }

    if (ch == '\'') {
      position++;
      return MakeSimpleToken(TokenType::Quote, "'", startLine, startColumn);
    }

    if (ch == '"') {
      string value;
      position++;

      while (position < (int)m_line.size()) {
        char current = m_line[position++];

        if (current == '"') {
          Token token;
          token.type = TokenType::String;
          token.input = value;
          token.output = "\"" + value + "\"";
          token.line = startLine;
          token.column = startColumn;
          return token;
        }

        if (current == '\\' && position < (int)m_line.size()) {
          char next = m_line[position++];
          if (next == 'n') value += '\n';
          else if (next == 't') value += '\t';
          else if (next == '"') value += '"';
          else if (next == '\\') value += '\\';
          else {
            value += '\\';
            value += next;
          }
        } else {
          value += current;
        }
      }

      return MakeSimpleToken(TokenType::NoClosingQuote, "", startLine,
                             (int)m_line.size() + 1);
    }

    string text;
    while (position < (int)m_line.size()) {
      char current = m_line[position];
      if (!IsPrintable(current) || IsSeparator(current)) {
        break;
      }

      text += current;
      position++;
    }

    return ClassifyTokenText(text, startLine, startColumn);
  }

  void DiscardRestOfLine(int line) {
    if (else_line && index_line == line) {
      position = (int)m_line.size();
    }
  }
};

class Parser {
 private:
  Lexer m_lexer;
  Token m_peek;
  bool m_hasPeek;

  Token PeekToken() {
    if (!m_hasPeek) {
      m_peek = m_lexer.NextToken();
      m_hasPeek = true;
    }

    return m_peek;
  }

  Token ConsumeToken() {
    Token token = PeekToken();
    m_hasPeek = false;
    return token;
  }

  error_message NormalizeError(error_message error, SourcePosition start) {
    if (error.type == ErrorType::NoMoreInput) {
      return error;
    }

    error_message normalized = error;
    normalized.line = error.line - start.line + 1;

    if (normalized.line <= 1) {
      normalized.line = 1;
      normalized.column = error.column - start.column + 1;
    }

    if (normalized.column < 1) {
      normalized.column = 1;
    }

    return normalized;
  }

  error_message MakeUnexpectedAtomOrLeftParen(Token token) {
    error_message error;
    error.type = ErrorType::UnexpectedAtomOrLeftParen;
    error.tokenText = token.output;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  error_message MakeUnexpectedRightParen(Token token) {
    error_message error;
    error.type = ErrorType::UnexpectedRightParen;
    error.tokenText = token.output;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  error_message MakeNoClosingQuote(Token token) {
    error_message error;
    error.type = ErrorType::NoClosingQuote;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  error_message MakeNoMoreInput() {
    error_message error;
    error.type = ErrorType::NoMoreInput;
    error.line = 0;
    error.column = 0;
    return error;
  }

  bool IsAtomToken(TokenType type) {
    return type == TokenType::Int || type == TokenType::Float ||
           type == TokenType::String || type == TokenType::Nil ||
           type == TokenType::True || type == TokenType::Symbol;
  }

  NodePtr MakeQuoteNode(NodePtr quoted) {
    vector<NodePtr> items;
    items.push_back(MakeNode(NodeType::Symbol, "quote"));
    items.push_back(quoted);
    return BuildList(items, MakeNil());
  }

  bool ParseSExp(NodePtr &result, error_message &error) {
    Token token = PeekToken();

    if (token.type == TokenType::EndOfFile) {
      error = MakeNoMoreInput();
      return false;
    }

    if (token.type == TokenType::NoClosingQuote) {
      error = MakeNoClosingQuote(token);
      return false;
    }

    if (IsAtomToken(token.type)) {
      ConsumeToken();
      result = MakeAtomFromToken(token);
      return true;
    }

    if (token.type == TokenType::Quote) {
      ConsumeToken();
      NodePtr quoted;
      if (!ParseSExp(quoted, error)) {
        return false;
      }

      result = MakeQuoteNode(quoted);
      return true;
    }

    if (token.type == TokenType::LeftParen) {
      ConsumeToken();

      Token next = PeekToken();
      if (next.type == TokenType::EndOfFile) {
        error = MakeNoMoreInput();
        return false;
      }

      if (next.type == TokenType::NoClosingQuote) {
        error = MakeNoClosingQuote(next);
        return false;
      }

      if (next.type == TokenType::RightParen) {
        ConsumeToken();
        result = MakeNil();
        return true;
      }

      vector<NodePtr> items;
      NodePtr first;
      if (!ParseSExp(first, error)) {
        return false;
      }
      items.push_back(first);

      while (true) {
        Token after = PeekToken();

        if (after.type == TokenType::EndOfFile) {
          error = MakeNoMoreInput();
          return false;
        }

        if (after.type == TokenType::NoClosingQuote) {
          error = MakeNoClosingQuote(after);
          return false;
        }

        if (after.type == TokenType::RightParen) {
          ConsumeToken();
          result = BuildList(items, MakeNil());
          return true;
        }

        if (after.type == TokenType::Dot) {
          ConsumeToken();
          NodePtr tail;
          if (!ParseSExp(tail, error)) {
            return false;
          }

          Token closing = PeekToken();
          if (closing.type == TokenType::EndOfFile) {
            error = MakeNoMoreInput();
            return false;
          }

          if (closing.type == TokenType::NoClosingQuote) {
            error = MakeNoClosingQuote(closing);
            return false;
          }

          if (closing.type != TokenType::RightParen) {
            error = MakeUnexpectedRightParen(closing);
            return false;
          }

          ConsumeToken();
          result = BuildList(items, tail);
          return true;
        }

        NodePtr item;
        if (!ParseSExp(item, error)) {
          return false;
        }
        items.push_back(item);
      }
    }

    error = MakeUnexpectedAtomOrLeftParen(token);
    return false;
  }

 public:
  struct ReadResult {
    bool success;
    bool cleanEof;
    NodePtr expression;
    error_message error;
  };

  Parser(string bufferedLine = "", bool hasBufferedLine = false) {
    m_lexer = Lexer(bufferedLine, hasBufferedLine);
    m_hasPeek = false;
  }

  ReadResult ReadTopLevelSExp() {
    ReadResult result;
    result.success = false;
    result.cleanEof = false;

    SourcePosition start = m_lexer.CurrentPosition();
    Token first = PeekToken();

    if (first.type == TokenType::EndOfFile) {
      result.cleanEof = true;
      return result;
    }

    if (first.type == TokenType::NoClosingQuote) {
      result.error = NormalizeError(MakeNoClosingQuote(first), start);
      m_lexer.DiscardRestOfLine(first.line);
      m_hasPeek = false;
      return result;
    }

    error_message error;
    NodePtr expression;
    if (!ParseSExp(expression, error)) {
      result.error = NormalizeError(error, start);
      if (error.line > 0) {
        m_lexer.DiscardRestOfLine(error.line);
      }
      m_hasPeek = false;
      return result;
    }

    result.success = true;
    result.expression = expression;
    return result;
  }
};

bool IsAtom(NodePtr node) {
  return node->type != NodeType::Cons;
}

bool IsNumber(NodePtr node) {
  return node->type == NodeType::Int || node->type == NodeType::Float;
}

bool IsFalseValue(NodePtr node) {
  return node->type == NodeType::Nil;
}

bool IsProperList(NodePtr node) {
  NodePtr current = node;
  while (current->type == NodeType::Cons) {
    current = current->right;
  }

  return current->type == NodeType::Nil;
}

vector<NodePtr> ListToVector(NodePtr node) {
  vector<NodePtr> items;
  NodePtr current = node;
  while (current->type == NodeType::Cons) {
    items.push_back(current->left);
    current = current->right;
  }

  return items;
}

double GetDoubleValue(NodePtr node) {
  if (node->type == NodeType::Int) {
    return (double)stoll(node->text);
  }

  return stod(node->text);
}

long long GetIntValue(NodePtr node) {
  return stoll(node->text);
}

string AtomToString(NodePtr node) {
  if (node->type == NodeType::Nil) return "nil";
  if (node->type == NodeType::True) return "#t";
  if (node->type == NodeType::String) return "\"" + node->text + "\"";
  if (node->type == NodeType::Int) {
    long long value = stoll(node->text);
    return to_string(value);
  }
  if (node->type == NodeType::Float) {
    double value = stod(node->text);
    ostringstream out;
    out << fixed << setprecision(3) << value;
    return out.str();
  }
  if (node->type == NodeType::Procedure) {
    return "#<procedure " + node->text + ">";
  }
  return node->text;
}

vector<string> FormatSExpLines(NodePtr node) {
  if (IsAtom(node)) {
    return {AtomToString(node)};
  }

  vector<string> lines;

  vector<NodePtr> items;
  NodePtr current = node;
  while (current->type == NodeType::Cons) {
    items.push_back(current->left);
    current = current->right;
  }
  NodePtr tail = current;

  auto addChildAsNewLine = [&](NodePtr child) {
    vector<string> childLines = FormatSExpLines(child);
    lines.push_back("  " + childLines[0]);
    for (size_t i = 1; i < childLines.size(); ++i) {
      lines.push_back("  " + childLines[i]);
    }
  };

  vector<string> firstLines = FormatSExpLines(items[0]);
  lines.push_back("( " + firstLines[0]);
  for (size_t i = 1; i < firstLines.size(); ++i) {
    lines.push_back("  " + firstLines[i]);
  }

  for (size_t i = 1; i < items.size(); ++i) {
    addChildAsNewLine(items[i]);
  }

  if (tail->type != NodeType::Nil) {
    lines.push_back("  .");
    addChildAsNewLine(tail);
  }

  lines.push_back(")");
  return lines;
}

void PrintSExp(NodePtr node, int indent) {
  vector<string> lines = FormatSExpLines(node);
  for (string &line : lines) {
    cout << string(indent, ' ') << line << "\n";
  }
}

bool IsExitExpression(NodePtr node) {
  if (node->type != NodeType::Cons) return false;
  if (!IsProperList(node)) return false;

  vector<NodePtr> items = ListToVector(node);
  if (items.size() != 1) return false;
  return items[0]->type == NodeType::Symbol && items[0]->text == "exit";
}

void PrintSyntaxError(error_message error) {
  if (error.type == ErrorType::UnexpectedAtomOrLeftParen) {
    cout << "ERROR (unexpected token) : atom or '(' expected when token at Line "
         << error.line << " Column " << error.column << " is >>"
         << error.tokenText << "<<" << endl;
  } else if (error.type == ErrorType::UnexpectedRightParen) {
    cout << "ERROR (unexpected token) : ')' expected when token at Line "
         << error.line << " Column " << error.column << " is >>"
         << error.tokenText << "<<" << endl;
  } else if (error.type == ErrorType::NoClosingQuote) {
    cout << "ERROR (no closing quote) : END-OF-LINE encountered at Line "
         << error.line << " Column " << error.column << endl;
  } else {
    cout << "ERROR (no more input) : END-OF-FILE encountered" << endl;
  }
}

string ValueToInlineString(NodePtr node) {
  if (IsAtom(node)) {
    return AtomToString(node);
  }

  vector<string> lines = FormatSExpLines(node);
  if (lines.empty()) return "";
  if (lines.size() == 1) return lines[0];
  return lines[0];
}

class Environment {
 private:
  map<string, NodePtr> m_primitives;
  map<string, NodePtr> m_userDefined;

 public:
  Environment() {
    vector<string> names = {
      "cons", "list", "quote", "define", "car", "cdr", "atom?", "pair?",
      "list?", "null?", "integer?", "real?", "number?", "string?",
      "boolean?", "symbol?", "+", "-", "*", "/", "not", "and", "or",
      ">", ">=", "<", "<=", "=", "string-append", "string>?",
      "string<?", "string=?", "eqv?", "equal?", "begin", "if", "cond",
      "clean-environment", "exit"
    };

    for (string name : names) {
      m_primitives[name] = MakeProcedure(name);
    }
  }

  NodePtr Lookup(string name) {
    if (m_userDefined.count(name)) return m_userDefined[name];
    if (m_primitives.count(name)) return m_primitives[name];
    return NULL;
  }

  bool IsPrimitiveName(string name) {
    return m_primitives.count(name) > 0;
  }

  void Define(string name, NodePtr value) {
    m_userDefined[name] = value;
  }

  void ClearUserDefinitions() {
    m_userDefined.clear();
  }
};

EvalError MakeEvalError(EvalErrorType type, string name = "", NodePtr node = NULL) {
  EvalError error;
  error.type = type;
  error.name = name;
  error.node = node;
  return error;
}

bool NodeEqual(NodePtr a, NodePtr b);

NodePtr Eval(NodePtr node, Environment &env, bool topLevel = false);

void EnsureProperListCall(NodePtr node) {
  if (!IsProperList(node)) {
    throw MakeEvalError(EvalErrorType::NonList, "", node);
  }
}

NodePtr EvalSymbol(NodePtr node, Environment &env) {
  NodePtr value = env.Lookup(node->text);
  if (value == NULL) {
    throw MakeEvalError(EvalErrorType::UnboundSymbol, node->text, node);
  }

  return value;
}

NodePtr MakeNumericNode(double value, bool useFloat) {
  if (!useFloat) {
    return MakeNode(NodeType::Int, to_string((long long)value));
  }

  ostringstream out;
  out << fixed << setprecision(15) << value;
  string text = out.str();
  while (!text.empty() && text.back() == '0') text.pop_back();
  if (!text.empty() && text.back() == '.') text.push_back('0');
  return MakeNode(NodeType::Float, text);
}

NodePtr MakeBooleanNode(bool value) {
  return value ? MakeTrue() : MakeNil();
}

NodePtr EvalSequence(const vector<NodePtr> &items, int startIndex, Environment &env) {
  NodePtr result = MakeNil();
  for (int i = startIndex; i < (int)items.size(); ++i) {
    result = Eval(items[i], env, false);
  }

  return result;
}

NodePtr EvalQuote(const vector<NodePtr> &items, NodePtr whole) {
  if ((int)items.size() != 2) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "quote", whole);
  }

  return items[1];
}

NodePtr EvalDefine(const vector<NodePtr> &items, NodePtr whole, Environment &env) {
  if ((int)items.size() != 3 || items[1]->type != NodeType::Symbol ||
      env.IsPrimitiveName(items[1]->text)) {
    throw MakeEvalError(EvalErrorType::DefineFormat, "", whole);
  }

  NodePtr value = Eval(items[2], env, false);
  env.Define(items[1]->text, value);
  return MakeMessage(items[1]->text + " defined");
}

NodePtr EvalIf(const vector<NodePtr> &items, NodePtr whole, Environment &env) {
  if ((int)items.size() != 3 && (int)items.size() != 4) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "if", whole);
  }

  NodePtr test = Eval(items[1], env, false);
  if (!IsFalseValue(test)) {
    return Eval(items[2], env, false);
  }

  if ((int)items.size() == 4) {
    return Eval(items[3], env, false);
  }

  throw MakeEvalError(EvalErrorType::NoReturnValue, "", whole);
}

NodePtr EvalBegin(const vector<NodePtr> &items, NodePtr whole, Environment &env) {
  if ((int)items.size() < 2) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "begin", whole);
  }

  return EvalSequence(items, 1, env);
}

NodePtr EvalAnd(const vector<NodePtr> &items, NodePtr whole, Environment &env) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "and", whole);
  }

  NodePtr result = MakeTrue();
  for (int i = 1; i < (int)items.size(); ++i) {
    result = Eval(items[i], env);
    if (IsFalseValue(result)) {
      return result;
    }
  }

  return result;
}

NodePtr EvalOr(const vector<NodePtr> &items, NodePtr whole, Environment &env) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "or", whole);
  }

  NodePtr result = MakeNil();
  for (int i = 1; i < (int)items.size(); ++i) {
    result = Eval(items[i], env);
    if (!IsFalseValue(result)) {
      return result;
    }
  }

  return result;
}

NodePtr EvalCond(const vector<NodePtr> &items, NodePtr whole, Environment &env) {
  if ((int)items.size() < 2) {
    throw MakeEvalError(EvalErrorType::CondFormat, "", whole);
  }

  for (int i = 1; i < (int)items.size(); ++i) {
    NodePtr clause = items[i];
    if (clause->type == NodeType::Nil || !IsProperList(clause)) {
      throw MakeEvalError(EvalErrorType::CondFormat, "", whole);
    }

    vector<NodePtr> parts = ListToVector(clause);
    if ((int)parts.size() < 2) {
      throw MakeEvalError(EvalErrorType::CondFormat, "", whole);
    }
  }

  for (int i = 1; i < (int)items.size(); ++i) {
    NodePtr clause = items[i];
    vector<NodePtr> parts = ListToVector(clause);

    bool isElseClause =
      parts[0]->type == NodeType::Symbol &&
      parts[0]->text == "else" &&
      i == (int)items.size() - 1;

    NodePtr testValue;
    if (isElseClause) {
      testValue = MakeTrue();
    } else {
      testValue = Eval(parts[0], env, false);
    }

    if (!IsFalseValue(testValue)) {
      return EvalSequence(parts, 1, env);
    }
  }

  throw MakeEvalError(EvalErrorType::NoReturnValue, "", whole);
}

void EnsureArgCount(string name, const vector<NodePtr> &args, int expected) {
  if ((int)args.size() != expected) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, name, NULL);
  }
}

void EnsureArgCountAtLeast(string name, const vector<NodePtr> &args, int minimum) {
  if ((int)args.size() < minimum) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, name, NULL);
  }
}

void EnsureNumberArg(string name, NodePtr value) {
  if (!IsNumber(value)) {
    throw MakeEvalError(EvalErrorType::IncorrectArgumentType, name, value);
  }
}

void EnsureStringArg(string name, NodePtr value) {
  if (value->type != NodeType::String) {
    throw MakeEvalError(EvalErrorType::IncorrectArgumentType, name, value);
  }
}

bool IsPrimitiveProcedureName(string name) {
  static vector<string> names = {
    "cons", "list", "car", "cdr", "atom?", "pair?", "list?", "null?",
    "integer?", "real?", "number?", "string?", "boolean?", "symbol?",
    "+", "-", "*", "/", "not", ">", ">=", "<", "<=", "=",
    "string-append", "string>?", "string<?", "string=?", "eqv?",
    "equal?", "clean-environment", "exit"
  };

  for (string primitive : names) {
    if (primitive == name) return true;
  }

  return false;
}

void ValidateProcedureCallShape(string name, int argCount, NodePtr whole, bool topLevel) {
  if (name == "clean-environment" && !topLevel) {
    throw MakeEvalError(EvalErrorType::LevelOfCleanEnvironment, "", whole);
  }
  if (name == "exit" && !topLevel) {
    throw MakeEvalError(EvalErrorType::LevelOfExit, "", whole);
  }

  if (name == "exit" || name == "car" || name == "cdr" || name == "atom?" ||
      name == "pair?" || name == "list?" || name == "null?" ||
      name == "integer?" || name == "real?" || name == "number?" ||
      name == "string?" || name == "boolean?" || name == "symbol?" ||
      name == "not" || name == "clean-environment") {
    if (argCount != 1 && name != "clean-environment" && name != "exit") {
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, name, NULL);
    }
    if ((name == "clean-environment" || name == "exit") && argCount != 0) {
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, name, NULL);
    }
    return;
  }

  if (name == "cons" || name == "define") {
    if (argCount != 2) {
      if (name == "define") throw MakeEvalError(EvalErrorType::DefineFormat, "", whole);
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, name, NULL);
    }
    return;
  }

  if (name == "quote") {
    if (argCount != 1) {
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "quote", NULL);
    }
    return;
  }

  if (name == "eqv?" || name == "equal?") {
    if (argCount != 2) {
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, name, NULL);
    }
    return;
  }

  if (name == "if") {
    if (argCount != 2 && argCount != 3) {
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "if", NULL);
    }
    return;
  }

  if (name == "begin") {
    if (argCount < 1) {
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "begin", NULL);
    }
    return;
  }

  if (name == "cond") {
    if (argCount < 1) {
      throw MakeEvalError(EvalErrorType::CondFormat, "", whole);
    }
    return;
  }

  if (name == "list") return;

  if (name == "+" || name == "-" || name == "*" || name == "/" ||
      name == "and" || name == "or" || name == ">" || name == ">=" ||
      name == "<" || name == "<=" || name == "=" || name == "string-append" ||
      name == "string>?" || name == "string<?" || name == "string=?") {
    if (argCount < 2) {
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, name, NULL);
    }
  }
}

NodePtr ApplyCons(const vector<NodePtr> &args) {
  EnsureArgCount("cons", args, 2);
  return MakeCons(args[0], args[1]);
}

NodePtr ApplyList(const vector<NodePtr> &args) {
  return BuildList(args, MakeNil());
}

NodePtr ApplyCar(const vector<NodePtr> &args) {
  EnsureArgCount("car", args, 1);
  if (args[0]->type != NodeType::Cons) {
    throw MakeEvalError(EvalErrorType::IncorrectArgumentType, "car", args[0]);
  }

  return args[0]->left;
}

NodePtr ApplyCdr(const vector<NodePtr> &args) {
  EnsureArgCount("cdr", args, 1);
  if (args[0]->type != NodeType::Cons) {
    throw MakeEvalError(EvalErrorType::IncorrectArgumentType, "cdr", args[0]);
  }

  return args[0]->right;
}

NodePtr ApplyPredicate(string name, const vector<NodePtr> &args) {
  EnsureArgCount(name, args, 1);
  NodePtr value = args[0];

  if (name == "atom?") return MakeBooleanNode(IsAtom(value));
  if (name == "pair?") return MakeBooleanNode(value->type == NodeType::Cons);
  if (name == "list?") return MakeBooleanNode(value->type == NodeType::Nil || IsProperList(value));
  if (name == "null?") return MakeBooleanNode(value->type == NodeType::Nil);
  if (name == "integer?") return MakeBooleanNode(value->type == NodeType::Int);
  if (name == "real?" || name == "number?") return MakeBooleanNode(IsNumber(value));
  if (name == "string?") return MakeBooleanNode(value->type == NodeType::String);
  if (name == "boolean?") return MakeBooleanNode(value->type == NodeType::Nil || value->type == NodeType::True);
  return MakeBooleanNode(value->type == NodeType::Symbol);
}

NodePtr ApplyAddSubMulDiv(string name, const vector<NodePtr> &args) {
  EnsureArgCountAtLeast(name, args, 2);
  bool hasFloat = false;
  for (NodePtr arg : args) {
    EnsureNumberArg(name, arg);
    if (arg->type == NodeType::Float) hasFloat = true;
  }

  if (name == "+") {
    if (!hasFloat) {
      long long total = 0;
      for (NodePtr arg : args) total += GetIntValue(arg);
      return MakeNode(NodeType::Int, to_string(total));
    }

    double total = 0.0;
    for (NodePtr arg : args) total += GetDoubleValue(arg);
    return MakeNode(NodeType::Float, to_string(total));
  }

  if (name == "-") {
    if (!hasFloat) {
      long long total = GetIntValue(args[0]);
      for (int i = 1; i < (int)args.size(); ++i) total -= GetIntValue(args[i]);
      return MakeNode(NodeType::Int, to_string(total));
    }

    double total = GetDoubleValue(args[0]);
    for (int i = 1; i < (int)args.size(); ++i) total -= GetDoubleValue(args[i]);
    return MakeNode(NodeType::Float, to_string(total));
  }

  if (name == "*") {
    if (!hasFloat) {
      long long total = 1;
      for (NodePtr arg : args) total *= GetIntValue(arg);
      return MakeNode(NodeType::Int, to_string(total));
    }

    double total = 1.0;
    for (NodePtr arg : args) total *= GetDoubleValue(arg);
    return MakeNode(NodeType::Float, to_string(total));
  }

  for (int i = 1; i < (int)args.size(); ++i) {
    if ((args[i]->type == NodeType::Int && GetIntValue(args[i]) == 0) ||
        (args[i]->type == NodeType::Float && fabs(GetDoubleValue(args[i])) < 1e-12)) {
      throw MakeEvalError(EvalErrorType::DivisionByZero, "/", NULL);
    }
  }

  if (!hasFloat) {
    long long total = GetIntValue(args[0]);
    for (int i = 1; i < (int)args.size(); ++i) total /= GetIntValue(args[i]);
    return MakeNode(NodeType::Int, to_string(total));
  }

  double total = GetDoubleValue(args[0]);
  for (int i = 1; i < (int)args.size(); ++i) total /= GetDoubleValue(args[i]);
  return MakeNode(NodeType::Float, to_string(total));
}

NodePtr ApplyNot(const vector<NodePtr> &args) {
  EnsureArgCount("not", args, 1);
  return MakeBooleanNode(IsFalseValue(args[0]));
}

NodePtr ApplyNumericCompare(string name, const vector<NodePtr> &args) {
  EnsureArgCountAtLeast(name, args, 2);
  for (NodePtr arg : args) EnsureNumberArg(name, arg);

  for (int i = 0; i + 1 < (int)args.size(); ++i) {
    double left = GetDoubleValue(args[i]);
    double right = GetDoubleValue(args[i + 1]);
    bool ok = false;

    if (name == ">") ok = left > right;
    else if (name == ">=") ok = left >= right;
    else if (name == "<") ok = left < right;
    else if (name == "<=") ok = left <= right;
    else ok = fabs(left - right) < 1e-12;

    if (!ok) return MakeNil();
  }

  return MakeTrue();
}

NodePtr ApplyStringAppend(const vector<NodePtr> &args) {
  EnsureArgCountAtLeast("string-append", args, 2);
  string result;
  for (NodePtr arg : args) {
    EnsureStringArg("string-append", arg);
    result += arg->text;
  }

  return MakeNode(NodeType::String, result);
}

NodePtr ApplyStringCompare(string name, const vector<NodePtr> &args) {
  EnsureArgCountAtLeast(name, args, 2);
  for (NodePtr arg : args) EnsureStringArg(name, arg);

  for (int i = 0; i + 1 < (int)args.size(); ++i) {
    string left = args[i]->text;
    string right = args[i + 1]->text;
    bool ok = false;

    if (name == "string>?") ok = left > right;
    else if (name == "string<?") ok = left < right;
    else ok = left == right;

    if (!ok) return MakeNil();
  }

  return MakeTrue();
}

bool NodeEqv(NodePtr a, NodePtr b) {
  if (a == b) return true;
  if (a->type == NodeType::String || b->type == NodeType::String) return false;
  if (a->type == NodeType::Cons || b->type == NodeType::Cons) return false;

  if (IsNumber(a) && IsNumber(b)) {
    return fabs(GetDoubleValue(a) - GetDoubleValue(b)) < 1e-12;
  }

  if (a->type != b->type) return false;
  return a->text == b->text;
}

bool NodeEqual(NodePtr a, NodePtr b) {
  if (a == b) return true;
  if (IsNumber(a) && IsNumber(b)) {
    return fabs(GetDoubleValue(a) - GetDoubleValue(b)) < 1e-12;
  }

  if (a->type != b->type) return false;
  if (a->type == NodeType::Cons) {
    return NodeEqual(a->left, b->left) && NodeEqual(a->right, b->right);
  }

  return a->text == b->text;
}

NodePtr ApplyEquality(string name, const vector<NodePtr> &args) {
  EnsureArgCount(name, args, 2);
  if (name == "eqv?") return MakeBooleanNode(NodeEqv(args[0], args[1]));
  return MakeBooleanNode(NodeEqual(args[0], args[1]));
}

NodePtr ApplyProcedure(string name, const vector<NodePtr> &args, NodePtr whole,
                       Environment &env) {
  if (name == "cons") return ApplyCons(args);
  if (name == "list") return ApplyList(args);
  if (name == "car") return ApplyCar(args);
  if (name == "cdr") return ApplyCdr(args);
  if (name == "atom?" || name == "pair?" || name == "list?" || name == "null?" ||
      name == "integer?" || name == "real?" || name == "number?" ||
      name == "string?" || name == "boolean?" || name == "symbol?") {
    return ApplyPredicate(name, args);
  }
  if (name == "+" || name == "-" || name == "*" || name == "/") {
    return ApplyAddSubMulDiv(name, args);
  }
  if (name == "not") return ApplyNot(args);
  if (name == ">" || name == ">=" || name == "<" || name == "<=" || name == "=") {
    return ApplyNumericCompare(name, args);
  }
  if (name == "string-append") return ApplyStringAppend(args);
  if (name == "string>?" || name == "string<?" || name == "string=?") {
    return ApplyStringCompare(name, args);
  }
  if (name == "eqv?" || name == "equal?") return ApplyEquality(name, args);
  if (name == "clean-environment") {
    EnsureArgCount("clean-environment", args, 0);
    env.ClearUserDefinitions();
    return MakeMessage("environment cleaned");
  }
  if (name == "exit") {
    EnsureArgCount("exit", args, 0);
    return MakeProcedure("exit");
  }

  throw MakeEvalError(EvalErrorType::AttemptToApplyNonFunction,
                      ValueToInlineString(MakeProcedure(name)), whole);
}

NodePtr Eval(NodePtr node, Environment &env, bool topLevel) {
  if (node->type == NodeType::Nil || node->type == NodeType::True ||
      node->type == NodeType::Int || node->type == NodeType::Float ||
      node->type == NodeType::String || node->type == NodeType::Procedure ||
      node->type == NodeType::Message) {
    return node;
  }

  if (node->type == NodeType::Symbol) {
    return EvalSymbol(node, env);
  }

  EnsureProperListCall(node);
  vector<NodePtr> items = ListToVector(node);
  if (items.empty()) return MakeNil();

  if (items[0]->type == NodeType::Symbol) {
    string name = items[0]->text;
    if (name == "define" && !topLevel) {
      throw MakeEvalError(EvalErrorType::LevelOfDefine, "", node);
    }
    if (name == "quote") return EvalQuote(items, node);
    if (name == "define") return EvalDefine(items, node, env);
    if (name == "if") return EvalIf(items, node, env);
    if (name == "cond") return EvalCond(items, node, env);
    if (name == "begin") return EvalBegin(items, node, env);
    if (name == "and") return EvalAnd(items, node, env);
    if (name == "or") return EvalOr(items, node, env);
  }

  NodePtr procedure = Eval(items[0], env, false);
  if (procedure->type != NodeType::Procedure) {
    throw MakeEvalError(EvalErrorType::AttemptToApplyNonFunction,
                        ValueToInlineString(procedure), procedure);
  }

  ValidateProcedureCallShape(procedure->text, (int)items.size() - 1, node, topLevel);

  vector<NodePtr> args;
  for (int i = 1; i < (int)items.size(); ++i) {
    args.push_back(Eval(items[i], env, false));
  }

  return ApplyProcedure(procedure->text, args, node, env);
}

void PrintEvalError(EvalError error) {
  if (error.type == EvalErrorType::NonList) {
    cout << "ERROR (non-list) : ";
    PrintSExp(error.node, 0);
  } else if (error.type == EvalErrorType::IncorrectNumberOfArguments) {
    cout << "ERROR (incorrect number of arguments) : " << error.name << endl;
  } else if (error.type == EvalErrorType::IncorrectArgumentType) {
    cout << "ERROR (" << error.name << " with incorrect argument type) : ";
    if (IsAtom(error.node)) {
      cout << ValueToInlineString(error.node) << endl;
    } else {
      PrintSExp(error.node, 0);
    }
  } else if (error.type == EvalErrorType::AttemptToApplyNonFunction) {
    cout << "ERROR (attempt to apply non-function) : ";
    if (IsAtom(error.node)) {
      cout << ValueToInlineString(error.node) << endl;
    } else {
      PrintSExp(error.node, 0);
    }
  } else if (error.type == EvalErrorType::NoReturnValue) {
    cout << "ERROR (no return value) : ";
    PrintSExp(error.node, 0);
  } else if (error.type == EvalErrorType::UnboundSymbol) {
    cout << "ERROR (unbound symbol) : " << error.name << endl;
  } else if (error.type == EvalErrorType::DivisionByZero) {
    cout << "ERROR (division by zero) : /" << endl;
  } else if (error.type == EvalErrorType::DefineFormat) {
    cout << "ERROR (DEFINE format) : ";
    PrintSExp(error.node, 0);
  } else if (error.type == EvalErrorType::LevelOfDefine) {
    cout << "ERROR (level of DEFINE)" << endl;
  } else if (error.type == EvalErrorType::LevelOfCleanEnvironment) {
    cout << "ERROR (level of CLEAN-ENVIRONMENT)" << endl;
  } else if (error.type == EvalErrorType::LevelOfExit) {
    cout << "ERROR (level of EXIT)" << endl;
  } else {
    cout << "ERROR (COND format) : ";
    PrintSExp(error.node, 0);
  }
}

int main() {
  string firstInputLine;
  bool hasBufferedFirstLine = false;

  if (getline(cin, firstInputLine)) {
    string trimmedFirstLine = Trim(firstInputLine);
    if (!trimmedFirstLine.empty() &&
        (unsigned char)trimmedFirstLine[0] == 0xEF &&
        trimmedFirstLine.size() >= 3 &&
        (unsigned char)trimmedFirstLine[1] == 0xBB &&
        (unsigned char)trimmedFirstLine[2] == 0xBF) {
      trimmedFirstLine = Trim(trimmedFirstLine.substr(3));
      firstInputLine = trimmedFirstLine;
    }

    bool isTestNumberLine = !trimmedFirstLine.empty();
    for (char ch : trimmedFirstLine) {
      if (!isdigit((unsigned char)ch)) {
        isTestNumberLine = false;
        break;
      }
    }

    if (!isTestNumberLine) {
      hasBufferedFirstLine = true;
    }
  } else {
    return 0;
  }

  cout << "Welcome to OurScheme!" << endl << endl;

  Parser parser(firstInputLine, hasBufferedFirstLine);
  Environment env;
  bool endedByExit = false;

  while (true) {
    cout << "> ";
    Parser::ReadResult result = parser.ReadTopLevelSExp();

    if (result.cleanEof) {
      PrintSyntaxError({ErrorType::NoMoreInput, "", 0, 0});
      break;
    }

    if (!result.success) {
      PrintSyntaxError(result.error);
      if (result.error.type == ErrorType::NoMoreInput) {
        break;
      }

      cout << endl;
      continue;
    }

    if (IsExitExpression(result.expression)) {
      endedByExit = true;
      break;
    }

    try {
      NodePtr value = Eval(result.expression, env, true);
      PrintSExp(value, 0);
    } catch (EvalError &error) {
      PrintEvalError(error);
    }

    cout << endl;
  }

  if (endedByExit) {
    cout << endl;
  }

  cout << "Thanks for using OurScheme!";
  return 0;
}
