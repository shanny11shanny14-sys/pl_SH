#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
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
  Cons,      //列表
  Procedure, //过程
  Message    //define / clean-environment 這種不用正常印 S-exp 的訊息
};

enum class ErrorType { //token / parser error
  UnexpectedAtomOrLeftParen,
  UnexpectedRightParen,
  NoClosingQuote,
  NoMoreInput
};

enum class EvalErrorType { //calculate error
  NonList,                    //非列表
  IncorrectNumberOfArguments, //参数数量错误
  IncorrectArgumentType,      //参数类型错误
  AttemptToApplyNonFunction,  //试图把非函数的东西当函数调用
  NoReturnValue,              //没有返回值
  UnboundSymbol,              //未绑定的符号
  UnboundParameter,           //参数没有值
  UnboundTestCondition,       //if / cond 的 test-condition 没有值
  UnboundCondition,           //and / or 的 condition 没有值
  DivisionByZero,             //除数为 0
  DefineFormat,               //define 格式错误
  LetFormat,                  //let 格式错误，Project 3 新增
  LambdaFormat,               //lambda 格式错误，Project 3 新增
  CondFormat,                 //cond 格式错误
  LevelOfDefine,              //define 只能在最外层用
  LevelOfCleanEnvironment,    //clean-environment 只能在最外层用
  LevelOfExit                 //exit 只能在最外层用
};

class Environment; //forward declaration

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
  vector<string> parameters;
  vector<Node*> body;
  shared_ptr<Environment> closureEnv;
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
  node->closureEnv = NULL;
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

NodePtr MakeClosure(string name, const vector<string> &parameters,
                    const vector<NodePtr> &body,
                    shared_ptr<Environment> closureEnv) {
  NodePtr node = MakeNode(NodeType::Procedure, name);
  node->parameters = parameters;
  node->body = body;
  node->closureEnv = closureEnv;
  return node;
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
    index_line = 0; //index_line 是目前讀到的行數
    position = 0; //position 是目前在這行的哪個位置
    else_line = false; //else_line 表示目前這行是否還可以繼續讀
    buffered_line = hasBufferedLine; //第一行如果不是測資編號，就先存起來
    end = false; //end 表示是否已經讀到檔案結尾
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

      if (m_line[position] == ';') { //註解後面的字都不用讀
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
      if (!IsPrintable(current) || IsSeparator(current)) { //遇到不能印的字元或分隔符號就停
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

vector<NodePtr> ListToVector(NodePtr node) { //把列表转换成 vector
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

  return stod(node->text); //把字符串转换成 double
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

vector<string> FormatSExpLines(NodePtr node);

void AddChildAsNewLine(vector<string> &lines, NodePtr child) {
  vector<string> childLines = FormatSExpLines(child);
  lines.push_back("  " + childLines[0]);
  for (size_t i = 1; i < childLines.size(); ++i) {
    lines.push_back("  " + childLines[i]);
  }
}

vector<string> FormatSExpLines(NodePtr node) {
  if (IsAtom(node)) {
    return {AtomToString(node)};
  }

  vector<string> lines;

  //把 list 裡面的東西拆出來，放到 vector 裡面
  vector<NodePtr> items;
  NodePtr current = node;
  while (current->type == NodeType::Cons) {
    items.push_back(current->left);
    current = current->right;
  }
  NodePtr tail = current;

  //把第一行的東西放在 "( " 後面，如果第一行有多行，剩下的行也接著印
  vector<string> firstLines = FormatSExpLines(items[0]);
  lines.push_back("( " + firstLines[0]);
  for (size_t i = 1; i < firstLines.size(); ++i) {
    lines.push_back("  " + firstLines[i]);
  }

  //處理第二個元素以後的元素，每個元素都放在新的一行
  for (size_t i = 1; i < items.size(); ++i) {
    AddChildAsNewLine(lines, items[i]);
  }

  //如果 tail 不是 nil，就代表是 dotted pair，要多印一個 "."
  if (tail->type != NodeType::Nil) {
    lines.push_back("  .");
    AddChildAsNewLine(lines, tail);
  }

  lines.push_back(")");
  return lines;
}

void PrintSExp(NodePtr node, int indent) { //把樹狀資料 node 轉成老師要求的格式並印出來
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

class Environment : public enable_shared_from_this<Environment> {
 private:
  map<string, NodePtr> m_bindings;
  shared_ptr<Environment> m_parent;

 public:
  explicit Environment(shared_ptr<Environment> parent = NULL) {
    m_parent = parent;
  }

  static bool IsPrimitiveName(string name) {
    static set<string> names = { //內建函数的名字，工具箱裡本來就有，不用先 define
      "cons", "list", "car", "cdr", "atom?", "pair?", "list?", "null?",
      "integer?", "real?", "number?", "string?", "boolean?", "symbol?",
      "+", "-", "*", "/", "not", ">", ">=", "<", "<=", "=",
      "string-append", "string>?", "string<?", "string=?", "eqv?",
      "equal?", "clean-environment", "exit", "verbose", "verbose?"
    };

    return names.count(name) > 0;
  }

  static bool IsReservedWord(string name) {
    static set<string> names = {
      "quote", "and", "or", "begin", "if", "cond",
      "define", "lambda", "set!", "let"
    };

    return names.count(name) > 0;
  }

  shared_ptr<Environment> Root() {
    if (m_parent == NULL) return shared_from_this();
    return m_parent->Root();
  }

  NodePtr Lookup(string name) {
    if (m_bindings.count(name)) return m_bindings[name];
    if (m_parent != NULL) return m_parent->Lookup(name);
    if (IsPrimitiveName(name)) return MakeProcedure(name); //primitive procedure 被查到時才做成 procedure
    return NULL;
  }

  void DefineLocal(string name, NodePtr value) {
    m_bindings[name] = value;
  }

  void DefineGlobal(string name, NodePtr value) {
    shared_ptr<Environment> root = Root();
    root->m_bindings[name] = value;
  }

  void ClearUserDefinitions() {
    Root()->m_bindings.clear();
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

NodePtr Eval(NodePtr node, shared_ptr<Environment> env, bool topLevel = false);

void EnsureProperListCall(NodePtr node) {
  if (!IsProperList(node)) {
    throw MakeEvalError(EvalErrorType::NonList, "", node);
  }
}

NodePtr EvalSymbol(NodePtr node, shared_ptr<Environment> env) { //把符号转换成它的值，如果符号没有绑定值，就报错
  NodePtr value = env->Lookup(node->text);
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

NodePtr EvalSequence(const vector<NodePtr> &items, int startIndex, shared_ptr<Environment> env) {
  //begin / cond / function body 都會照順序算，最後一個結果才是回傳值
  NodePtr result = NULL;
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

bool IsUserSymbol(NodePtr node) { //Project 3：使用者自己取的名字，不能撞到 primitive 或 reserved word
  if (node->type != NodeType::Symbol) return false;
  if (Environment::IsPrimitiveName(node->text)) return false;
  if (Environment::IsReservedWord(node->text)) return false;
  return true;
}

bool ReadParameterItems(const vector<NodePtr> &nodes, vector<string> &parameters) {
  //Project 3：檢查 function/lambda 的參數，不能重複，也不能用保留字
  set<string> seen;
  for (NodePtr parameter : nodes) {
    if (!IsUserSymbol(parameter) || seen.count(parameter->text) > 0) {
      return false;
    }

    seen.insert(parameter->text);
    parameters.push_back(parameter->text);
  }

  return true;
}

bool ReadParameterList(NodePtr node, vector<string> &parameters) {
  if (!IsProperList(node)) return false;
  return ReadParameterItems(ListToVector(node), parameters);
}

NodePtr MakeLambdaProcedure(string name,
                            const vector<string> &parameters,
                            const vector<NodePtr> &body,
                            shared_ptr<Environment> env) {
  return MakeClosure(name, parameters, body, env->Root());
}

NodePtr EvalLambda(const vector<NodePtr> &items, NodePtr whole,
                   shared_ptr<Environment> env) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalErrorType::LambdaFormat, "", whole);
  }

  vector<string> parameters;
  if (!ReadParameterList(items[1], parameters)) {
    throw MakeEvalError(EvalErrorType::LambdaFormat, "", whole);
  }

  vector<NodePtr> body(items.begin() + 2, items.end());
  return MakeLambdaProcedure("lambda", parameters, body, env);
}

NodePtr EvalSymbolDefine(const vector<NodePtr> &items, NodePtr whole,
                         shared_ptr<Environment> env) {
  if ((int)items.size() != 3 || !IsUserSymbol(items[1])) {
    throw MakeEvalError(EvalErrorType::DefineFormat, "", whole);
  }

  NodePtr value = Eval(items[2], env, false);
  if (value == NULL) {
    throw MakeEvalError(EvalErrorType::NoReturnValue, "", items[2]);
  }

  env->DefineGlobal(items[1]->text, value);
  return MakeMessage(items[1]->text + " defined");
}

NodePtr EvalFunctionDefine(const vector<NodePtr> &items, NodePtr whole,
                           shared_ptr<Environment> env) {
  if (!IsProperList(items[1])) {
    throw MakeEvalError(EvalErrorType::DefineFormat, "", whole);
  }

  vector<NodePtr> functionHeader = ListToVector(items[1]);
  if (functionHeader.empty() || !IsUserSymbol(functionHeader[0])) {
    throw MakeEvalError(EvalErrorType::DefineFormat, "", whole);
  }

  vector<string> parameters;
  vector<NodePtr> parameterItems;
  for (int i = 1; i < (int)functionHeader.size(); ++i) {
    parameterItems.push_back(functionHeader[i]);
  }

  if (!ReadParameterItems(parameterItems, parameters)) {
    throw MakeEvalError(EvalErrorType::DefineFormat, "", whole);
  }

  vector<NodePtr> body(items.begin() + 2, items.end());
  NodePtr value = MakeLambdaProcedure(functionHeader[0]->text, parameters, body, env);
  env->DefineGlobal(functionHeader[0]->text, value);
  return MakeMessage(functionHeader[0]->text + " defined");
}

NodePtr EvalDefine(const vector<NodePtr> &items, NodePtr whole, shared_ptr<Environment> env) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalErrorType::DefineFormat, "", whole);
  }

  if (items[1]->type == NodeType::Symbol) {
    return EvalSymbolDefine(items, whole, env);
  }

  return EvalFunctionDefine(items, whole, env);
}

NodePtr EvalIf(const vector<NodePtr> &items, NodePtr whole, shared_ptr<Environment> env) {
  if ((int)items.size() != 3 && (int)items.size() != 4) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "if", whole);
  }

  NodePtr test = Eval(items[1], env, false);
  if (test == NULL) {
    throw MakeEvalError(EvalErrorType::UnboundTestCondition, "", items[1]);
  }
  if (!IsFalseValue(test)) {
    return Eval(items[2], env, false);
  }

  if ((int)items.size() == 4) {
    return Eval(items[3], env, false);
  }

  return NULL;
}

NodePtr EvalBegin(const vector<NodePtr> &items, NodePtr whole, shared_ptr<Environment> env) {
  if ((int)items.size() < 2) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "begin", whole);
  }

  return EvalSequence(items, 1, env);
}

NodePtr EvalAnd(const vector<NodePtr> &items, NodePtr whole, shared_ptr<Environment> env) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "and", whole);
  }

  NodePtr result = MakeTrue();
  for (int i = 1; i < (int)items.size(); ++i) {
    result = Eval(items[i], env);
    if (result == NULL) {
      throw MakeEvalError(EvalErrorType::UnboundCondition, "", items[i]);
    }
    if (IsFalseValue(result)) {
      return result;
    }
  }

  return result;
}

NodePtr EvalOr(const vector<NodePtr> &items, NodePtr whole, shared_ptr<Environment> env) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, "or", whole);
  }

  NodePtr result = MakeNil();
  for (int i = 1; i < (int)items.size(); ++i) {
    result = Eval(items[i], env);
    if (result == NULL) {
      throw MakeEvalError(EvalErrorType::UnboundCondition, "", items[i]);
    }
    if (!IsFalseValue(result)) {
      return result;
    }
  }

  return result;
}

NodePtr EvalCond(const vector<NodePtr> &items, NodePtr whole, shared_ptr<Environment> env) {
  if ((int)items.size() < 2) {
    throw MakeEvalError(EvalErrorType::CondFormat, "", whole);
  }

  for (int i = 1; i < (int)items.size(); ++i) {
    NodePtr clause = items[i]; //條件區塊
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
      if (testValue == NULL) {
        throw MakeEvalError(EvalErrorType::UnboundTestCondition, "", parts[0]);
      }
    }

    if (!IsFalseValue(testValue)) {
      return EvalSequence(parts, 1, env);
    }
  }

  return NULL;
}

NodePtr EvalLet(const vector<NodePtr> &items, NodePtr whole, shared_ptr<Environment> env) {
  if ((int)items.size() < 3 || !IsProperList(items[1])) {
    throw MakeEvalError(EvalErrorType::LetFormat, "", whole);
  }

  vector<NodePtr> bindings = ListToVector(items[1]); //let 的第一段 binding list
  vector<pair<string, NodePtr> > bindingsToEvaluate; //先記名字和還沒算的 expression
  set<string> seen;

  for (NodePtr binding : bindings) {
    if (!IsProperList(binding)) {
      throw MakeEvalError(EvalErrorType::LetFormat, "", whole);
    }

    vector<NodePtr> pairItems = ListToVector(binding);
    if ((int)pairItems.size() != 2 || !IsUserSymbol(pairItems[0]) ||
        seen.count(pairItems[0]->text) > 0) {
      throw MakeEvalError(EvalErrorType::LetFormat, "", whole);
    }

    seen.insert(pairItems[0]->text);
    bindingsToEvaluate.push_back(make_pair(pairItems[0]->text, pairItems[1]));
  }

  vector<pair<string, NodePtr> > values; //binding 的右邊要在外層環境先算完
  for (size_t i = 0; i < bindingsToEvaluate.size(); ++i) {
    NodePtr value = Eval(bindingsToEvaluate[i].second, env, false);
    if (value == NULL) {
      throw MakeEvalError(EvalErrorType::NoReturnValue, "", bindingsToEvaluate[i].second);
    }
    values.push_back(make_pair(bindingsToEvaluate[i].first, value));
  }

  shared_ptr<Environment> localEnv(new Environment(env)); //let 的 local environment
  for (size_t i = 0; i < values.size(); ++i) {
    localEnv->DefineLocal(values[i].first, values[i].second);
  }

  return EvalSequence(items, 2, localEnv);
}

void EnsureArgCount(string name, const vector<NodePtr> &args, int expected) {
  if ((int)args.size() != expected) { //(int)size_t，unsigned int 轉換成 int
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

bool g_verbose = true;

void ValidateProcedureCallShape(string name, int argCount, NodePtr whole, bool topLevel) {
  //先檢查 primitive procedure 的參數數量，有錯就不要再去算參數
  if (name == "clean-environment" && !topLevel) {
    throw MakeEvalError(EvalErrorType::LevelOfCleanEnvironment, "", whole);
  }
  if (name == "exit" && !topLevel) {
    throw MakeEvalError(EvalErrorType::LevelOfExit, "", whole);
  }
  if (name == "verbose?") {
    if (argCount != 0) {
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, name, NULL);
    }
    return;
  }

  //只能 1 個參數的 primitive procedure
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

  if (name == "verbose") {
    if (argCount != 1) {
      throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments, name, NULL);
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

  //operation == "/"
  for (int i = 1; i < (int)args.size(); ++i) {
    //判断除数是否为 0，fabs() 函数返回一个数的绝对值
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

NodePtr ApplyNumericCompare(string name, const vector<NodePtr> &args) { //數字大小比較
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
    else ok = fabs(left - right) < 1e-12; //浮點數比較會有誤差，所以用 fabs

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

    if (name == "string>?") ok = left > right; //choose one comparer based on the name
    else if (name == "string<?") ok = left < right;
    else ok = left == right;

    if (!ok) return MakeNil();
  }

  return MakeTrue();
}

bool NodeEqv(NodePtr a, NodePtr b) { //比較數值是不是一樣，如果是 string 或 cons 就直接 false
  if (a == b) return true;
  if (a->type == NodeType::String || b->type == NodeType::String) return false;
  if (a->type == NodeType::Cons || b->type == NodeType::Cons) return false;

  if (IsNumber(a) && IsNumber(b)) {
    return fabs(GetDoubleValue(a) - GetDoubleValue(b)) < 1e-12; //fabs 是為了處理浮點數誤差
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
                       shared_ptr<Environment> env) {
  //根据 procedure 的名字来判断是哪个函数，然后把参数传入對應的函數計算結果
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
    env->ClearUserDefinitions();
    return MakeMessage("environment cleaned");
  }
  if (name == "verbose") {
    EnsureArgCount("verbose", args, 1);
    g_verbose = !IsFalseValue(args[0]);
    return g_verbose ? MakeTrue() : MakeNil();
  }
  if (name == "verbose?") {
    EnsureArgCount("verbose?", args, 0);
    return g_verbose ? MakeTrue() : MakeNil();
  }
  if (name == "exit") {
    EnsureArgCount("exit", args, 0);
    return MakeProcedure("exit");
  }

  throw MakeEvalError(EvalErrorType::AttemptToApplyNonFunction,
                      ValueToInlineString(MakeProcedure(name)), whole);
}

NodePtr ApplyUserProcedure(NodePtr procedure, const vector<NodePtr> &args) {
  if ((int)args.size() != (int)procedure->parameters.size()) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments,
                        procedure->text, NULL);
  }

  shared_ptr<Environment> callEnv(new Environment(procedure->closureEnv)); //function call 的 local environment
  for (size_t i = 0; i < procedure->parameters.size(); ++i) {
    callEnv->DefineLocal(procedure->parameters[i], args[i]);
  }

  return EvalSequence(procedure->body, 0, callEnv);
}

bool IsDefineOrCleanExpression(NodePtr node) {
  if (node->type != NodeType::Cons || !IsProperList(node)) return false;

  vector<NodePtr> items = ListToVector(node);
  if (items.empty() || items[0]->type != NodeType::Symbol) return false;
  return items[0]->text == "define" || items[0]->text == "clean-environment";
}

NodePtr Eval(NodePtr node, shared_ptr<Environment> env, bool topLevel) {
  //如果是原子节点，直接返回
  if (node->type == NodeType::Nil || node->type == NodeType::True ||
      node->type == NodeType::Int || node->type == NodeType::Float ||
      node->type == NodeType::String || node->type == NodeType::Procedure ||
      node->type == NodeType::Message) {
    return node;
  }

  if (node->type == NodeType::Symbol) {
    //只有 atom 是符号才需要查环境变量，例如 (define x 10) 之后 x 就要查表
    return EvalSymbol(node, env);
  }

  EnsureProperListCall(node);
  vector<NodePtr> items = ListToVector(node); //把列表转换成 vector
  if (items.empty()) return MakeNil();

  //如果列表的第一个元素是符号，就根据符号的名字来判断是不是特殊形式
  if (items[0]->type == NodeType::Symbol) {
    string name = items[0]->text;
    if (name == "define" && !topLevel) {
      throw MakeEvalError(EvalErrorType::LevelOfDefine, "", node);
    }
    if (name == "quote") return EvalQuote(items, node);
    if (name == "define") return EvalDefine(items, node, env);
    if (name == "lambda") return EvalLambda(items, node, env);
    if (name == "let") return EvalLet(items, node, env);
    if (name == "if") return EvalIf(items, node, env);
    if (name == "cond") return EvalCond(items, node, env);
    if (name == "begin") return EvalBegin(items, node, env);
    if (name == "and") return EvalAnd(items, node, env);
    if (name == "or") return EvalOr(items, node, env);
  }

  NodePtr procedure = Eval(items[0], env, false);
  if (procedure == NULL) {
    throw MakeEvalError(EvalErrorType::NoReturnValue, "", items[0]);
  }
  if (procedure->type != NodeType::Procedure) {
    throw MakeEvalError(EvalErrorType::AttemptToApplyNonFunction,
                        ValueToInlineString(procedure), procedure);
  }

  if (procedure->body.empty()) {
    ValidateProcedureCallShape(procedure->text, (int)items.size() - 1, node, topLevel);
  } else if ((int)items.size() - 1 != (int)procedure->parameters.size()) {
    throw MakeEvalError(EvalErrorType::IncorrectNumberOfArguments,
                        procedure->text, NULL);
  }

  vector<NodePtr> args;
  for (int i = 1; i < (int)items.size(); ++i) {
    NodePtr value = Eval(items[i], env, false); //把参数 items 都计算出来，放在 args 里
    if (value == NULL) {
      throw MakeEvalError(EvalErrorType::UnboundParameter, "", items[i]);
    }

    args.push_back(value);
  }

  if (!procedure->body.empty()) {
    return ApplyUserProcedure(procedure, args);
  }

  return ApplyProcedure(procedure->text, args, node, env);
}

void PrintEvalError(EvalError error) { //Eval 錯誤的類型有很多種，根據不同的類型打印不同的錯誤信息
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
  } else if (error.type == EvalErrorType::UnboundParameter) {
    cout << "ERROR (unbound parameter) : ";
    PrintSExp(error.node, 0);
  } else if (error.type == EvalErrorType::UnboundTestCondition) {
    cout << "ERROR (unbound test-condition) : ";
    PrintSExp(error.node, 0);
  } else if (error.type == EvalErrorType::UnboundCondition) {
    cout << "ERROR (unbound condition) : ";
    PrintSExp(error.node, 0);
  } else if (error.type == EvalErrorType::DivisionByZero) {
    cout << "ERROR (division by zero) : /" << endl;
  } else if (error.type == EvalErrorType::DefineFormat) {
    cout << "ERROR (DEFINE format) : ";
    PrintSExp(error.node, 0);
  } else if (error.type == EvalErrorType::LetFormat) {
    cout << "ERROR (LET format) : ";
    PrintSExp(error.node, 0);
  } else if (error.type == EvalErrorType::LambdaFormat) {
    cout << "ERROR (LAMBDA format) : ";
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
  shared_ptr<Environment> env(new Environment());
  bool endedByExit = false;

  while (true) {
    cout << "> ";
    Parser::ReadResult result = parser.ReadTopLevelSExp();

    if (result.cleanEof) {
      PrintSyntaxError({ErrorType::NoMoreInput, "", 0, 0});
      break;
    }

    if (!result.success) {
      PrintSyntaxError(result.error); //tokenText is used in some error messages
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
      if (value == NULL) {
        throw MakeEvalError(EvalErrorType::NoReturnValue, "", result.expression);
      }

      if (value->type == NodeType::Message) {
        if (g_verbose && IsDefineOrCleanExpression(result.expression)) {
          cout << value->text << endl;
        }
      } else {
        PrintSExp(value, 0);
      }
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
