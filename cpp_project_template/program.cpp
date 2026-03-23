#include <cctype>
#include <iomanip>
#include <iostream>
#include <memory>
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
  Cons
};

enum class ErrorType {
  UnexpectedAtomOrLeftParen,
  UnexpectedRightParen,
  NoClosingQuote,
  NoMoreInput
};

struct Token {
  TokenType type;
  string text;
  string display;
  int line;
  int column;
};

struct Node {
  NodeType type;
  string text;
  shared_ptr<Node> left;
  shared_ptr<Node> right;
};

struct ParseError {
  ErrorType type;
  string tokenText;
  int line;
  int column;
};

using NodePtr = shared_ptr<Node>;

NodePtr MakeNode(NodeType type, const string &text = "") {
  NodePtr node = make_shared<Node>();
  node->type = type;
  node->text = text;
  return node;
}

NodePtr MakeNil() {
  return MakeNode(NodeType::Nil, "nil");
}

NodePtr MakeTrue() {
  return MakeNode(NodeType::True, "#t");
}

NodePtr MakeCons(const NodePtr &left, const NodePtr &right) {
  NodePtr node = MakeNode(NodeType::Cons);
  node->left = left;
  node->right = right;
  return node;
}

NodePtr MakeAtomFromToken(const Token &token) {
  if (token.type == TokenType::Nil) return MakeNil();
  if (token.type == TokenType::True) return MakeTrue();
  if (token.type == TokenType::Int) return MakeNode(NodeType::Int, token.text);
  if (token.type == TokenType::Float) return MakeNode(NodeType::Float, token.text);
  if (token.type == TokenType::String) return MakeNode(NodeType::String, token.text);
  return MakeNode(NodeType::Symbol, token.text);
}

NodePtr BuildList(const vector<NodePtr> &items, const NodePtr &tail) {
  NodePtr result = tail;
  for (int i = static_cast<int>(items.size()) - 1; i >= 0; --i) {
    result = MakeCons(items[i], result);
  }

  return result;
}

bool IsSeparator(char ch) {
  return isspace(static_cast<unsigned char>(ch)) || ch == '(' || ch == ')' ||
         ch == '\'' || ch == '"' || ch == ';';
}

bool IsPrintable(char ch) {
  return isprint(static_cast<unsigned char>(ch)) != 0;
}

string Trim(const string &text) {
  int left = 0;
  int right = static_cast<int>(text.size()) - 1;

  while (left <= right && isspace(static_cast<unsigned char>(text[left]))) {
    left++;
  }

  while (right >= left && isspace(static_cast<unsigned char>(text[right]))) {
    right--;
  }

  if (left > right) return "";
  return text.substr(left, right - left + 1);
}

bool IsIntToken(const string &text) {
  if (text.empty()) return false;

  int index = 0;
  if (text[index] == '+' || text[index] == '-') {
    index++;
  }

  if (index >= static_cast<int>(text.size())) return false;

  for (int i = index; i < static_cast<int>(text.size()); ++i) {
    if (!isdigit(static_cast<unsigned char>(text[i]))) {
      return false;
    }
  }

  return true;
}

bool IsFloatToken(const string &text) {
  if (text.empty()) return false;

  int index = 0;
  if (text[index] == '+' || text[index] == '-') {
    index++;
  }

  if (index >= static_cast<int>(text.size())) return false;

  int dotCount = 0;
  int digitCount = 0;
  for (int i = index; i < static_cast<int>(text.size()); ++i) {
    char ch = text[i];
    if (ch == '.') {
      dotCount++;
      if (dotCount > 1) return false;
    } else if (isdigit(static_cast<unsigned char>(ch))) {
      digitCount++;
    } else {
      return false;
    }
  }

  if (dotCount != 1 || digitCount == 0) return false;

  return text != ".";
}

Token ClassifyTokenText(const string &text, int line, int column) {
  Token token;
  token.text = text;
  token.display = text;
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
  int m_lineNo;
  int m_pos;
  bool m_hasLine;
  bool m_hasBufferedLine;
  bool m_eof;

  bool ReadNextLine() {
    if (m_hasBufferedLine) {
      m_line = m_bufferedLine;
      m_hasBufferedLine = false;
    } else {
      if (!getline(cin, m_line)) {
        m_hasLine = false;
        m_eof = true;
        return false;
      }
    }

    m_lineNo++;
    m_pos = 0;
    m_hasLine = true;
    return true;
  }

  Token MakeSimpleToken(TokenType type, const string &text, int line, int column) {
    Token token;
    token.type = type;
    token.text = text;
    token.display = text;
    token.line = line;
    token.column = column;
    return token;
  }

 public:
  Lexer(const string &bufferedLine = "", bool hasBufferedLine = false)
      : m_bufferedLine(bufferedLine),
        m_lineNo(0),
        m_pos(0),
        m_hasLine(false),
        m_hasBufferedLine(hasBufferedLine),
        m_eof(false) {}

  Token NextToken() {
    while (true) {
      if ((!m_hasLine || m_pos >= static_cast<int>(m_line.size())) && !ReadNextLine()) {
        return MakeSimpleToken(TokenType::EndOfFile, "", m_lineNo + 1, 1);
      }

      while (m_pos < static_cast<int>(m_line.size()) &&
             isspace(static_cast<unsigned char>(m_line[m_pos]))) {
        m_pos++;
      }

      if (m_pos >= static_cast<int>(m_line.size())) {
        continue;
      }

      if (m_line[m_pos] == ';') {
        m_pos = static_cast<int>(m_line.size());
        continue;
      }

      break;
    }

    int startLine = m_lineNo;
    int startColumn = m_pos + 1;
    char ch = m_line[m_pos];

    if (ch == '(') {
      m_pos++;
      return MakeSimpleToken(TokenType::LeftParen, "(", startLine, startColumn);
    }

    if (ch == ')') {
      m_pos++;
      return MakeSimpleToken(TokenType::RightParen, ")", startLine, startColumn);
    }

    if (ch == '\'') {
      m_pos++;
      return MakeSimpleToken(TokenType::Quote, "'", startLine, startColumn);
    }

    if (ch == '"') {
      string value;
      m_pos++;

      while (m_pos < static_cast<int>(m_line.size())) {
        char current = m_line[m_pos++];

        if (current == '"') {
          Token token;
          token.type = TokenType::String;
          token.text = value;
          token.display = "\"" + value + "\"";
          token.line = startLine;
          token.column = startColumn;
          return token;
        }

        if (current == '\\' && m_pos < static_cast<int>(m_line.size())) {
          char next = m_line[m_pos++];
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
                             static_cast<int>(m_line.size()) + 1);
    }

    string text;
    while (m_pos < static_cast<int>(m_line.size())) {
      char current = m_line[m_pos];
      if (!IsPrintable(current) || IsSeparator(current)) {
        break;
      }

      text += current;
      m_pos++;
    }

    return ClassifyTokenText(text, startLine, startColumn);
  }

  void DiscardRestOfLine(int line) {
    if (m_hasLine && m_lineNo == line) {
      m_pos = static_cast<int>(m_line.size());
    }
  }

  bool IsAtCleanEof() const {
    return m_eof;
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

  ParseError NormalizeError(const ParseError &error, const Token &startToken) {
    if (error.type == ErrorType::NoMoreInput) {
      return error;
    }

    ParseError normalized = error;
    normalized.line = error.line - startToken.line + 1;

    if (normalized.line <= 1) {
      normalized.line = 1;
      normalized.column = error.column - startToken.column + 1;
    }

    if (normalized.column < 1) {
      normalized.column = 1;
    }

    return normalized;
  }

  ParseError MakeUnexpectedAtomOrLeftParen(const Token &token) {
    ParseError error;
    error.type = ErrorType::UnexpectedAtomOrLeftParen;
    error.tokenText = token.display;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  ParseError MakeUnexpectedRightParen(const Token &token) {
    ParseError error;
    error.type = ErrorType::UnexpectedRightParen;
    error.tokenText = token.display;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  ParseError MakeNoClosingQuote(const Token &token) {
    ParseError error;
    error.type = ErrorType::NoClosingQuote;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  ParseError MakeNoMoreInput() {
    ParseError error;
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

  NodePtr MakeQuoteNode(const NodePtr &quoted) {
    vector<NodePtr> items;
    items.push_back(MakeNode(NodeType::Symbol, "quote"));
    items.push_back(quoted);
    return BuildList(items, MakeNil());
  }

  bool ParseSExp(NodePtr &result, ParseError &error) {
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
    ParseError error;
  };

  Parser(const string &bufferedLine = "", bool hasBufferedLine = false)
      : m_lexer(bufferedLine, hasBufferedLine), m_hasPeek(false) {}

  ReadResult ReadTopLevelSExp() {
    ReadResult result;
    result.success = false;
    result.cleanEof = false;

    Token first = PeekToken();
    if (first.type == TokenType::EndOfFile) {
      result.cleanEof = true;
      return result;
    }

    if (first.type == TokenType::NoClosingQuote) {
      result.error = NormalizeError(MakeNoClosingQuote(first), first);
      m_lexer.DiscardRestOfLine(first.line);
      m_hasPeek = false;
      return result;
    }

    ParseError error;
    NodePtr expression;
    if (!ParseSExp(expression, error)) {
      result.error = NormalizeError(error, first);
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

bool IsAtom(const NodePtr &node) {
  return node->type != NodeType::Cons;
}

string EscapeString(const string &text) {
  string result;
  for (char ch : text) {
    if (ch == '\n') result += "\\n";
    else if (ch == '\t') result += "\\t";
    else if (ch == '"') result += "\\\"";
    else if (ch == '\\') result += "\\\\";
    else result += ch;
  }

  return result;
}

string AtomToString(const NodePtr &node) {
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
  return node->text;
}

void PrintSExp(const NodePtr &node, int indent);

void PrintListTail(const NodePtr &node, int indent) {
  NodePtr current = node;
  bool first = true;

  while (current->type == NodeType::Cons) {
    if (first) {
      if (IsAtom(current->left)) {
        cout << " " << AtomToString(current->left);
      } else {
        cout << "\n";
        PrintSExp(current->left, indent + 1);
      }
      first = false;
    } else {
      cout << "\n";
      if (IsAtom(current->left)) {
        cout << string(indent + 1, ' ') << AtomToString(current->left);
      } else {
        PrintSExp(current->left, indent + 1);
      }
    }

    current = current->right;
  }

  if (current->type != NodeType::Nil) {
    cout << "\n" << string(indent + 1, ' ') << ".";
    cout << "\n";
    if (IsAtom(current)) {
      cout << string(indent + 1, ' ') << AtomToString(current);
    } else {
      PrintSExp(current, indent + 1);
    }
  }
}

void PrintSExp(const NodePtr &node, int indent) {
  if (IsAtom(node)) {
    cout << string(indent, ' ') << AtomToString(node);
    return;
  }

  cout << string(indent, ' ') << "(";
  PrintListTail(node, indent);
  cout << "\n" << string(indent, ' ') << ")";
}

bool IsExitExpression(const NodePtr &node) {
  if (node->type != NodeType::Cons) return false;
  if (node->left->type != NodeType::Symbol || node->left->text != "exit") return false;
  return node->right->type == NodeType::Nil;
}

void PrintError(const ParseError &error) {
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

int main() {
  string firstInputLine;
  bool hasBufferedFirstLine = false;

  if (getline(cin, firstInputLine)) {
    string trimmedFirstLine = Trim(firstInputLine);
    if (!trimmedFirstLine.empty() &&
        static_cast<unsigned char>(trimmedFirstLine[0]) == 0xEF &&
        trimmedFirstLine.size() >= 3 &&
        static_cast<unsigned char>(trimmedFirstLine[1]) == 0xBB &&
        static_cast<unsigned char>(trimmedFirstLine[2]) == 0xBF) {
      trimmedFirstLine = Trim(trimmedFirstLine.substr(3));
    }

    if (trimmedFirstLine != "1") {
      hasBufferedFirstLine = true;
    }
  } else {
    return 0;
  }

  cout << "Welcome to OurScheme!" << endl;

  Parser parser(firstInputLine, hasBufferedFirstLine);
  while (true) {
    cout << "> ";
    Parser::ReadResult result = parser.ReadTopLevelSExp();

    if (result.cleanEof) {
      PrintError({ErrorType::NoMoreInput, "", 0, 0});
      break;
    }

    if (!result.success) {
      PrintError(result.error);
      if (result.error.type == ErrorType::NoMoreInput) {
        break;
      }
      continue;
    }

    if (IsExitExpression(result.expression)) {
      break;
    }

    PrintSExp(result.expression, 0);
    cout << endl;
  }

  cout << "Thanks for using OurScheme!" << endl;
  return 0;
}
