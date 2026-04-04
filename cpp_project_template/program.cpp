#include <cctype> // test
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum class TokenType {
  LeftParen,      // "("
  RightParen,     // ")"
  Dot,            // "."
  Quote,          // "'"
  Int,            // This is a whole number like 123.
  Float,          // This is a number with a dot like 3.14.
  String,         // This is a word inside " ".
  Nil,            // nil or false
  True,           // true
  Symbol,         // This is a normal name like abc.
  EndOfFile,      // no more input
  NoClosingQuote  // a string forgot its last ".
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
//=======================================================================
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

//=======================================================================
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
  Lexer(string bufferedLine = "", bool hasBufferedLine = false)
  {
    m_bufferedLine = bufferedLine;
    index_line = 0;
    position = 0;
    else_line = false;
    buffered_line = hasBufferedLine;
    end = false;
  }

  SourcePosition CurrentPosition() {
    if (else_line && position < (int)m_line.size()) {
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

  bool IsAtCleanEof() {
    return end;
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

  Parser(string bufferedLine = "", bool hasBufferedLine = false)
  {
    m_lexer = Lexer(bufferedLine, hasBufferedLine);
    m_hasPeek = false;
  }

  ReadResult ReadTopLevelSExp() {
    ReadResult result;
    result.success = false;
    result.cleanEof = false;

    SourcePosition start = m_lexer.CurrentPosition();
    Token first = PeekToken();

    if (first.type != TokenType::EndOfFile && first.line > start.line) {
      start = {first.line, 1};
    }

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

string EscapeString(string text) {
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
  return node->text;
}

vector<string> FormatSExpLines(NodePtr node) {
  if (IsAtom(node)) {
    return { AtomToString(node) };
  }

  vector<string> lines;

  vector<NodePtr> items;
  NodePtr current = node;
  while (current->type == NodeType::Cons) {
    items.push_back(current->left);
    current = current->right;
  }
  NodePtr tail = current;  // This is the tail. For a normal list, it is nil.

  auto addChildAsNewLine = [&](NodePtr child) {
    vector<string> childLines = FormatSExpLines(child);
    lines.push_back(" " + childLines[0]);
    for (size_t i = 1; i < childLines.size(); ++i) {
      lines.push_back(" " + childLines[i]);
    }
  };

  // Put the first item right after "( ".
  vector<string> firstLines = FormatSExpLines(items[0]);
  lines.push_back("( " + firstLines[0]);
  for (size_t i = 1; i < firstLines.size(); ++i) {
    lines.push_back(" " + firstLines[i]);
  }

  // Put each next item on a new line.
  for (size_t i = 1; i < items.size(); ++i) {
    addChildAsNewLine(items[i]);
  }

  // If this is a dotted pair, print the dot and the tail.
  if (tail->type != NodeType::Nil) {
    lines.push_back(" .");
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

void DeleteTree(NodePtr node) {
  if (node == NULL) return;

  DeleteTree(node->left);
  DeleteTree(node->right);
  delete node;
}

bool IsExitExpression(NodePtr node) {
  if (node->type != NodeType::Cons) return false;
  if (node->left->type != NodeType::Symbol || node->left->text != "exit") return false;
  return node->right->type == NodeType::Nil;
}

void PrintError(error_message error) {
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
  cout << "test";
  if (getline(cin, firstInputLine)) {
    string trimmedFirstLine = Trim(firstInputLine);
    if (!trimmedFirstLine.empty() &&
        (unsigned char)trimmedFirstLine[0] == 0xEF &&
        trimmedFirstLine.size() >= 3 &&
        (unsigned char)trimmedFirstLine[1] == 0xBB &&
        (unsigned char)trimmedFirstLine[2] == 0xBF) {
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
      DeleteTree(result.expression);
      break;
    }

    PrintSExp(result.expression, 0);
    DeleteTree(result.expression);
  }

  cout << "Thanks for using OurScheme!" << endl;
  return 0;
}
