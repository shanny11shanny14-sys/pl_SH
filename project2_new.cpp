#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum class Token_Type {
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
  No_Closing_Quote
};

enum class Node_Type {
  Nil,
  True,
  Int,
  Float,
  String,
  Symbol,
  Cons,//列表
  Procedure,//过程
  Message//错误信息
};

enum class TokenError_Type { //token error
  UnexpectedAtom_Or_LeftParen,
  Unexpected_RightParen,
  No_Closing_Quote,
  No_More_Input
};

enum class EvalError_Type { //caulculate error
  NonList,//非列表
  Wrong_NumberOfArguments,//参数数量错误
  Wrong_ArgumentType,//参数类型错误
  Calling_something_not_function_as_function,// 试图把非函数的东西当函数调用
  NoReturnValue,//没有返回值
  UnboundSymbol,//未绑定的符号
  DivisionByZero,//除数为0
  DefineFormat,//define格式错误
  CondFormat,//cond格式错误

  LevelOfDefine,//define 只能在最外層用
  LevelOfCleanEnvironment,//clean-environment 只能在最外層用
  LevelOfExit//exit 只能在最外層用  //(+ 1 (exit)) would be an error
};

struct Token {
  Token_Type type;
  string input;
  string output;
  int line;
  int column;
};

struct Node_data {
  Node_Type type;
  string text;
  Node_data *left;
  Node_data *right;
};

struct token_error_message {
  TokenError_Type type; 
  string tokenText;
  int line;
  int column;
};

struct line_column {
  int line;
  int column;
};

struct EvalError {
  EvalError_Type type;
  string name;
  Node_data *node;
};

using Node = Node_data*;

//forward declaration
class Environment;
vector<string> PrettyPrint(Node node);
EvalError MakeEvalError(EvalError_Type type, string name = "", Node node = NULL);
Node Eval(Node node, Environment &env, bool topLevel = false);


bool NodeEqual(Node a, Node b);
Node MakeNode(Node_Type type, string text = "") {
  Node node(new Node_data());
  node->type = type;
  node->text = text;
  node->left = NULL;
  node->right = NULL;
  return node;
}

Node MakeNil() {
  return MakeNode(Node_Type::Nil, "nil");
}

Node MakeTrue() {
  return MakeNode(Node_Type::True, "#t");
}

Node MakeProcedure(string name) {
  return MakeNode(Node_Type::Procedure, name);
}

Node MakeMessage(string text) {
  return MakeNode(Node_Type::Message, text);
}

Node MakeCons(Node left, Node right) {
  Node node = MakeNode(Node_Type::Cons);
  node->left = left;
  node->right = right;
  return node;
}

Node MakeAtomFromToken(Token token) {
  if (token.type == Token_Type::Nil) return MakeNil();
  if (token.type == Token_Type::True) return MakeTrue();
  if (token.type == Token_Type::Int) return MakeNode(Node_Type::Int, token.input);
  if (token.type == Token_Type::Float) return MakeNode(Node_Type::Float, token.input);
  if (token.type == Token_Type::String) return MakeNode(Node_Type::String, token.input);
  return MakeNode(Node_Type::Symbol, token.input);
}

Node BuildList(vector<Node> items, Node tail) {
  Node result = tail;
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


bool IsInt(string text) {
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

bool IsFloat(string text) {
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

Token Sort_TokenType(string text, int line, int column) {
  Token token;
  token.input = text;
  token.output = text;
  token.line = line;
  token.column = column;

  if (text == ".") token.type = Token_Type::Dot;
  else if (text == "nil" || text == "#f") token.type = Token_Type::Nil;
  else if (text == "t" || text == "#t") token.type = Token_Type::True;
  else if (IsInt(text)) token.type = Token_Type::Int;
  else if (IsFloat(text)) token.type = Token_Type::Float;
  else token.type = Token_Type::Symbol;

  return token;
}

class Lexer {
 private:
  string Line;
  int index_line;
  int position;
  bool useful_line;
  bool end;

  bool ReadNextLine() {
    if (!getline(cin, Line)) {
      useful_line = false;
      end = true;
      return false;
    }

    index_line++;
    position = 0;
    useful_line = true;
    return true;
  }

  Token MakeQuickToken(Token_Type type, string text, int line, int column) {
    Token token;
    token.type = type;
    token.input = text;
    token.output = text;
    token.line = line;
    token.column = column;
    return token;
  }

 public:
  Lexer() {
    index_line = 0;
    position = 0;
    useful_line = false;
    end = false;
  }

  line_column FindStartPlace() {
    if (useful_line) {
      int probe = position;
      while (probe < (int)Line.size() &&
             isspace((unsigned char)Line[probe])) {
        probe++;
      }

      if (probe >= (int)Line.size() || Line[probe] == ';') {
        return {index_line + 1, 1};
      }

      return {index_line, position + 1};
    }

    return {index_line + 1, 1};
  }

  Token NextToken() {
    while (true) {
      if ((!useful_line || position >= (int)Line.size()) && !ReadNextLine()) {
        return MakeQuickToken(Token_Type::EndOfFile, "", index_line + 1, 1);
      }

      while (position < (int)Line.size() &&
             isspace((unsigned char)Line[position])) {
        position++;
      }

      if (position >= (int)Line.size()) {
        continue;
      }

      if (Line[position] == ';') {
        position = (int)Line.size();
        continue;
      }

      break;
    }

    int startLine = index_line;
    int startColumn = position + 1;
    char ch = Line[position];

    if (ch == '(') {
      position++;
      return MakeQuickToken(Token_Type::LeftParen, "(", startLine, startColumn);
    }

    if (ch == ')') {
      position++;
      return MakeQuickToken(Token_Type::RightParen, ")", startLine, startColumn);
    }

    if (ch == '\'') {
      position++;
      return MakeQuickToken(Token_Type::Quote, "'", startLine, startColumn);
    }

    if (ch == '"') {
      string value;
      position++;

      while (position < (int)Line.size()) {
        char current = Line[position++];

        if (current == '"') {
          Token token;
          token.type = Token_Type::String;
          token.input = value;
          token.output = "\"" + value + "\"";
          token.line = startLine;
          token.column = startColumn;
          return token;
        }

        if (current == '\\' && position < (int)Line.size()) {
          char next = Line[position++];
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

      return MakeQuickToken(Token_Type::No_Closing_Quote, "", startLine,
                             (int)Line.size() + 1);
    }

    string text;
    while (position < (int)Line.size()) {
      char current = Line[position];
      if (!IsPrintable(current) || IsSeparator(current)) {
        break;
      }

      text += current;
      position++;
    }

    return Sort_TokenType(text, startLine, startColumn);
  }

  void DiscardRestOfLine(int line) {
    if (useful_line && index_line == line) {
      position = (int)Line.size();
    }
  }
};

class Parser {
 private:
  Lexer lexer;
  Token peek;
  bool peeked;

  Token PeekToken() {
    if (!peeked) {
      peek = lexer.NextToken();
      peeked = true;
    }

    return peek;
  }

  Token Use_Token() {
    Token token = PeekToken();
    peeked = false;
    return token;
  }

  token_error_message Right_token_error_message(token_error_message error, line_column start) {
    if (error.type == TokenError_Type::No_More_Input) {
      return error;
    }

    token_error_message normalized = error;
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

  token_error_message MakeUnexpectedAtom_Or_LeftParen(Token token) {
    token_error_message error;
    error.type = TokenError_Type::UnexpectedAtom_Or_LeftParen;
    error.tokenText = token.output;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  token_error_message MakeUnexpected_RightParen(Token token) {
    token_error_message error;
    error.type = TokenError_Type::Unexpected_RightParen;
    error.tokenText = token.output;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  token_error_message MakeNo_Closing_Quote(Token token) {
    token_error_message error;
    error.type = TokenError_Type::No_Closing_Quote;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  token_error_message MakeNo_More_Input() {
    token_error_message error;
    error.type = TokenError_Type::No_More_Input;
    error.line = 0;
    error.column = 0;
    return error;
  }

  bool IsAtomToken(Token_Type type) {
    return type == Token_Type::Int || type == Token_Type::Float ||
           type == Token_Type::String || type == Token_Type::Nil ||
           type == Token_Type::True || type == Token_Type::Symbol;
  }

  Node MakeQuoteNode(Node quoted) {
    vector<Node> items;
    items.push_back(MakeNode(Node_Type::Symbol, "quote"));
    items.push_back(quoted);
    return BuildList(items, MakeNil());
  }

  bool ParseSExp(Node &result, token_error_message &error) {
    Token token = PeekToken();

    if (token.type == Token_Type::EndOfFile) {
      error = MakeNo_More_Input();
      return false;
    }

    if (token.type == Token_Type::No_Closing_Quote) {
      error = MakeNo_Closing_Quote(token);
      return false;
    }

    if (IsAtomToken(token.type)) {
      Use_Token();
      result = MakeAtomFromToken(token);
      return true;
    }

    if (token.type == Token_Type::Quote) {
      Use_Token();
      Node quoted;
      if (!ParseSExp(quoted, error)) {
        return false;
      }

      result = MakeQuoteNode(quoted);
      return true;
    }

    if (token.type == Token_Type::LeftParen) {
      Use_Token();

      Token next = PeekToken();
      if (next.type == Token_Type::EndOfFile) {
        error = MakeNo_More_Input();
        return false;
      }

      if (next.type == Token_Type::No_Closing_Quote) {
        error = MakeNo_Closing_Quote(next);
        return false;
      }

      if (next.type == Token_Type::RightParen) {
        Use_Token();
        result = MakeNil();
        return true;
      }

      vector<Node> items;
      Node first;
      if (!ParseSExp(first, error)) {
        return false;
      }
      items.push_back(first);

      while (true) {
        Token after = PeekToken();

        if (after.type == Token_Type::EndOfFile) {
          error = MakeNo_More_Input();
          return false;
        }

        if (after.type == Token_Type::No_Closing_Quote) {
          error = MakeNo_Closing_Quote(after);
          return false;
        }

        if (after.type == Token_Type::RightParen) {
          Use_Token();
          result = BuildList(items, MakeNil());
          return true;
        }

        if (after.type == Token_Type::Dot) {
          Use_Token();
          Node tail;
          if (!ParseSExp(tail, error)) {
            return false;
          }

          Token closing = PeekToken();
          if (closing.type == Token_Type::EndOfFile) {
            error = MakeNo_More_Input();
            return false;
          }

          if (closing.type == Token_Type::No_Closing_Quote) {
            error = MakeNo_Closing_Quote(closing);
            return false;
          }

          if (closing.type != Token_Type::RightParen) {
            error = MakeUnexpected_RightParen(closing);
            return false;
          }

          Use_Token();
          result = BuildList(items, tail);
          return true;
        }

        Node item;
        if (!ParseSExp(item, error)) {
          return false;
        }
        items.push_back(item);
      }
    }

    error = MakeUnexpectedAtom_Or_LeftParen(token);
    return false;
  }

 public:
  struct Result {
    bool success;
    bool cleanEof;
    Node expression;
    token_error_message error;
  };

  Parser() {
    lexer = Lexer();
    peeked = false;
  }

  Result Read() {
    Result result;
    result.success = false;
    result.cleanEof = false;

    line_column start = lexer.FindStartPlace();
    Token first = PeekToken();

    if (first.type == Token_Type::EndOfFile) {
      result.cleanEof = true;
      return result;
    }

    if (first.type == Token_Type::No_Closing_Quote) {
      result.error = Right_token_error_message(MakeNo_Closing_Quote(first), start);
      lexer.DiscardRestOfLine(first.line);
      peeked = false;
      return result;
    }

    token_error_message error;
    Node expression;
    if (!ParseSExp(expression, error)) {
      result.error = Right_token_error_message(error, start);
      if (error.line > 0) {
        lexer.DiscardRestOfLine(error.line);
      }
      peeked = false;
      return result;
    }

    result.success = true;
    result.expression = expression;
    return result;
  }
};

bool IsAtom(Node node) {
  return node->type != Node_Type::Cons;
}

bool IsNumber(Node node) {
  return node->type == Node_Type::Int || node->type == Node_Type::Float;
}

bool IsFalseValue(Node node) {
  return node->type == Node_Type::Nil;
}

bool IsProperList(Node node) {
  Node current = node;
  while (current->type == Node_Type::Cons) {
    current = current->right;
  }

  return current->type == Node_Type::Nil;
}

vector<Node> ListToVector(Node node) { //把列表转换成vector
  vector<Node> items;
  Node current = node;
  while (current->type == Node_Type::Cons) {
    items.push_back(current->left);
    current = current->right;
  }

  return items;
}

double GetDoubleValue(Node node) {
  if (node->type == Node_Type::Int) {
    return (double)stoll(node->text);
  }

  return stod(node->text);//把字符串转换成双精度浮点数
}

long long GetIntValue(Node node) {
  return stoll(node->text); //把字符串转换成长整数
}

string AtomToString(Node node) {
  if (node->type == Node_Type::Nil) return "nil";
  if (node->type == Node_Type::True) return "#t";
  if (node->type == Node_Type::String) return "\"" + node->text + "\"";
  if (node->type == Node_Type::Int) {
    long long value = stoll(node->text);
    return to_string(value);
  }
  if (node->type == Node_Type::Float) {
    double value = stod(node->text);
    ostringstream out;
    out << fixed << setprecision(3) << value;
    return out.str();
  }
  if (node->type == Node_Type::Procedure) {
    return "#<procedure " + node->text + ">";
  }
  return node->text;
}


vector<string> PrettyPrint(Node node);

void AddChildAsNewLine(Node child, vector<string> &lines) {
  vector<string> childLines = PrettyPrint(child);
  lines.push_back("  " + childLines[0]);
  for (size_t i = 1; i < childLines.size(); ++i) {
    lines.push_back("  " + childLines[i]);
  }
}

vector<string> PrettyPrint(Node node) {
  if (IsAtom(node)) {
    return {AtomToString(node)};
  }

  vector<string> lines;

  //把 list 裡面的東西拆出來,放到 vector 裡面
  vector<Node> items;
  Node current = node;
  while (current->type == Node_Type::Cons) { 
    items.push_back(current->left);
    current = current->right;
  }
  Node tail = current;


  vector<string> firstLines = PrettyPrint(items[0]);
  lines.push_back("( " + firstLines[0]);
  //把第一行的东西放在 "( " 后面,如果第一行有多行,就把剩下的行也放在下面,like list > ((1 2) 3)
  for (size_t i = 1; i < firstLines.size(); ++i) {
    lines.push_back("  " + firstLines[i]);
  }

  //處理第二個元素以後的元素,每個元素都放在新的一行,並且在前面加上兩個空格
  for (size_t i = 1; i < items.size(); ++i) {
    AddChildAsNewLine(items[i], lines);
  }

  //如果 tail 不是 nil, 就在倒数第二行加上 " .", 然后把 tail 也放在新的一行
  if (tail->type != Node_Type::Nil) {
    lines.push_back("  .");
    AddChildAsNewLine(tail, lines);
  }

  lines.push_back(")");
  return lines;
}

void PrintSExp(Node node) {// 把樹狀資料 node 轉成老師要求的格式並印出來
  vector<string> lines = PrettyPrint(node);
  for (string &line : lines) {
    cout << line << "\n";
  }
}

bool IsExit(Node node) {
  if (node->type != Node_Type::Cons) return false;
  if (!IsProperList(node)) return false;

  vector<Node> items = ListToVector(node);
  if (items.size() != 1) return false;
  return items[0]->type == Node_Type::Symbol 
      && items[0]->text == "exit";
}

void Print_token_error_message(token_error_message error) {//打印token error
  if (error.type == TokenError_Type::UnexpectedAtom_Or_LeftParen) {
    cout << "ERROR (unexpected token) : atom or '(' expected when token at Line "
         << error.line << " Column " 
         << error.column << " is >>"
         << error.tokenText << "<<" << endl;

  } else if (error.type == TokenError_Type::Unexpected_RightParen) {
    cout << "ERROR (unexpected token) : ')' expected when token at Line "
         << error.line << " Column " 
         << error.column << " is >>"
         << error.tokenText << "<<" << endl;

  } else if (error.type == TokenError_Type::No_Closing_Quote) {
    cout << "ERROR (no closing quote) : END-OF-LINE encountered at Line "
         << error.line << " Column " 
         << error.column << endl;

  } else {
    cout << "ERROR (no more input) : END-OF-FILE encountered" << endl;
  }
}


class Environment {
 private:
  map<string, Node> original;
  map<string, Node> user_defined;

 public:
  Environment() {
    vector<string> names = { //內建函数的名字,工具箱裡本來就有,不用先寫：(define + ...)
      "cons", "list", "quote", "define", "car", "cdr", "atom?", "pair?",
      "list?", "null?", "integer?", "real?", "number?", "string?",
      "boolean?", "symbol?", "+", "-", "*", "/", "not", "and", "or",
      ">", ">=", "<", "<=", "=", "string-append", "string>?",
      "string<?", "string=?", "eqv?", "equal?", "begin", "if", "cond",
      "clean-environment", "exit"
    };

    for (string name : names) {
      original[name] = MakeProcedure(name);
    }
  }

  Node Lookup(string name) {
    if (user_defined.count(name)) return user_defined[name];
    if (original.count(name)) return original[name];
    return NULL;
  }

  bool IsPrimitiveName(string name) {
    return original.count(name) > 0;
  }

  void Define(string name, Node value) {
    user_defined[name] = value;
  }

  void ClearUserDefinitions() {
    user_defined.clear();
  }
};

EvalError MakeEvalError(EvalError_Type type, string name = "", Node node = NULL) {
  EvalError error;
  error.type = type;
  error.name = name;
  error.node = node;
  return error;
}


void EnsureProperListCall(Node node) {
  if (!IsProperList(node)) {
    throw MakeEvalError(EvalError_Type::NonList, "", node);
  }
}

Node EvalSymbol(Node node, Environment &env) {//把符号转换成它的值,如果符号没有绑定值,就报错
  Node value = env.Lookup(node->text);
  if (value == NULL) {
    throw MakeEvalError(EvalError_Type::UnboundSymbol, node->text, node);
  }

  return value;
}

Node MakeNumericNode(double value, bool useFloat) {
  if (!useFloat) {
    return MakeNode(Node_Type::Int, to_string((long long)value));
  }

  ostringstream out;
  out << fixed << setprecision(15) << value;
  string text = out.str();
  while (!text.empty() && text.back() == '0') text.pop_back();
  if (!text.empty() && text.back() == '.') text.push_back('0');
  return MakeNode(Node_Type::Float, text);
}

Node MakeBooleanNode(bool value) {
  return value ? MakeTrue() : MakeNil();
}

Node EvalSequence( vector<Node> &items, int startIndex, Environment &env) {
  Node result = MakeNil();
  for (int i = startIndex; i < (int)items.size(); ++i) {
    result = Eval(items[i], env, false);
  }

  return result;
}

Node EvalQuote( vector<Node> &items, Node whole) {
  if ((int)items.size() != 2) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "quote", whole);
  }

  return items[1];
}

Node EvalDefine( vector<Node> &items, Node whole, Environment &env) {
  if ((int)items.size() != 3 || items[1]->type != Node_Type::Symbol ||
      env.IsPrimitiveName(items[1]->text)) {
    throw MakeEvalError(EvalError_Type::DefineFormat, "", whole);
  }

  Node value = Eval(items[2], env, false);
  env.Define(items[1]->text, value);
  return MakeMessage(items[1]->text + " defined");
}

Node EvalIf( vector<Node> &items, Node whole, Environment &env) {
  if ((int)items.size() != 3 && (int)items.size() != 4) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "if", whole);
  }

  Node test = Eval(items[1], env, false);
  if (!IsFalseValue(test)) {
    return Eval(items[2], env, false);
  }

  if ((int)items.size() == 4) {
    return Eval(items[3], env, false);
  }

  throw MakeEvalError(EvalError_Type::NoReturnValue, "", whole);
}

Node EvalBegin( vector<Node> &items, Node whole, Environment &env) {
  if ((int)items.size() < 2) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "begin", whole);
  }

  return EvalSequence(items, 1, env);
}

Node EvalAnd( vector<Node> &items, Node whole, Environment &env) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "and", whole);
  }

  Node result = MakeTrue();
  for (int i = 1; i < (int)items.size(); ++i) {
    result = Eval(items[i], env);
    if (IsFalseValue(result)) {
      return result;
    }
  }

  return result;
}

Node EvalOr( vector<Node> &items, Node whole, Environment &env) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "or", whole);
  }

  Node result = MakeNil();
  for (int i = 1; i < (int)items.size(); ++i) {
    result = Eval(items[i], env);
    if (!IsFalseValue(result)) {
      return result;
    }
  }

  return result;
}

Node EvalCond( vector<Node> &items, Node whole, Environment &env) {
  if ((int)items.size() < 2) {
    throw MakeEvalError(EvalError_Type::CondFormat, "", whole);
  }

  for (int i = 1; i < (int)items.size(); ++i) {
    Node IF = items[i];  //條件區塊
    if (IF->type == Node_Type::Nil || !IsProperList(IF)) {
      throw MakeEvalError(EvalError_Type::CondFormat, "", whole);
    }

    vector<Node> parts = ListToVector(IF);
    if ((int)parts.size() < 2) {
      throw MakeEvalError(EvalError_Type::CondFormat, "", whole);
    }
  }

  for (int i = 1; i < (int)items.size(); ++i) {
    Node IF = items[i];
    vector<Node> parts = ListToVector(IF);

    bool isElseIF =
      parts[0]->type == Node_Type::Symbol &&
      parts[0]->text == "else" &&
      i == (int)items.size() - 1;

    Node testValue;
    if (isElseIF) testValue = MakeTrue();
    else testValue = Eval(parts[0], env, false);

    if (!IsFalseValue(testValue))  return EvalSequence(parts, 1, env);
    
  }

  throw MakeEvalError(EvalError_Type::NoReturnValue, "", whole);
}

void Counts_size(string name,  vector<Node> &counts, int expected) {
  if ((int)counts.size() != expected) {  //(int)size_t ,unsigned int 转换成int
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, name, NULL);
  }
}

void CountsSize_ATLeast(string name,  vector<Node> &counts, int min) {
  if ((int)counts.size() < min) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, name, NULL);
  }
}

void IsNumber(string name, Node value) {
  if (!IsNumber(value)) {
    throw MakeEvalError(EvalError_Type::Wrong_ArgumentType, name, value);
  }
}

void Type_String(string name, Node value) {
  if (value->type != Node_Type::String) {
    throw MakeEvalError(EvalError_Type::Wrong_ArgumentType, name, value);
  }
}


void Valid(string name, int countCount, Node whole, bool topLevel) {
  if (name == "clean-environment" && !topLevel) 
    throw MakeEvalError(EvalError_Type::LevelOfCleanEnvironment, "", whole);
  
  if (name == "exit" && !topLevel) 
    throw MakeEvalError(EvalError_Type::LevelOfExit, "", whole);
  
  if (name == "clean-environment" || name == "exit") {
    if (countCount != 0) 
      throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, name, NULL);

    return;
  }

  // 只能 1 個參數的 primitive procedure
  if (name == "car" || name == "cdr" || name == "atom?" ||
      name == "pair?" || name == "list?" || name == "null?" ||
      name == "integer?" || name == "real?" || name == "number?" ||
      name == "string?" || name == "boolean?" || name == "symbol?" ||
      name == "not") {
    if (countCount != 1) {
      throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, name, NULL);
    }
    return;
  }

  if (name == "cons" || name == "define") {
    if (countCount != 2) {
      if (name == "define") throw MakeEvalError(EvalError_Type::DefineFormat, "", whole);
      throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, name, NULL);
    }
    return;
  }

  if (name == "quote") {
    if (countCount != 1) {
      throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "quote", NULL);
    }
    return;
  }

  if (name == "eqv?" || name == "equal?") {
    if (countCount != 2) {
      throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, name, NULL);
    }
    return;
  }

  if (name == "if") {
    if (countCount != 2 && countCount != 3) {
      throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "if", NULL);
    }
    return;
  }

  if (name == "begin") {
    if (countCount < 1) {
      throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "begin", NULL);
    }
    return;
  }

  if (name == "cond") {
    if (countCount < 1) {
      throw MakeEvalError(EvalError_Type::CondFormat, "", whole);
    }
    return;
  }

  if (name == "list") return;

  if (name == "+" || name == "-" || name == "*" || name == "/" ||
      name == "and" || name == "or" || name == ">" || name == ">=" ||
      name == "<" || name == "<=" || name == "=" || name == "string-append" ||
      name == "string>?" || name == "string<?" || name == "string=?") {
    if (countCount < 2) {
      throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, name, NULL);
    }
  }
}

Node GO_Cons( vector<Node> &counts) {
  Counts_size("cons", counts, 2);
  return MakeCons(counts[0], counts[1]);
}

Node GO_List( vector<Node> &counts) {
  return BuildList(counts, MakeNil());
}

Node GO_Car( vector<Node> &counts) {
  Counts_size("car", counts, 1);
  if (counts[0]->type != Node_Type::Cons) {
    throw MakeEvalError(EvalError_Type::Wrong_ArgumentType, "car", counts[0]);
  }

  return counts[0]->left;
}

Node GO_Cdr( vector<Node> &counts) {
  Counts_size("cdr", counts, 1);
  if (counts[0]->type != Node_Type::Cons) {
    throw MakeEvalError(EvalError_Type::Wrong_ArgumentType, "cdr", counts[0]);
  }

  return counts[0]->right;
}

Node GO_Predicate(string name,  vector<Node> &counts) {
  Counts_size(name, counts, 1);
  Node value = counts[0];

  if (name == "atom?") return MakeBooleanNode(IsAtom(value));
  if (name == "pair?") return MakeBooleanNode(value->type == Node_Type::Cons);
  if (name == "list?") return MakeBooleanNode(value->type == Node_Type::Nil || IsProperList(value));
  if (name == "null?") return MakeBooleanNode(value->type == Node_Type::Nil);
  if (name == "integer?") return MakeBooleanNode(value->type == Node_Type::Int);
  if (name == "real?" || name == "number?") return MakeBooleanNode(IsNumber(value));
  if (name == "string?") return MakeBooleanNode(value->type == Node_Type::String);
  if (name == "boolean?") return MakeBooleanNode(value->type == Node_Type::Nil || value->type == Node_Type::True);
  return MakeBooleanNode(value->type == Node_Type::Symbol);
}

Node GO_AddSubMulDiv(string operation,  vector<Node> &counts) { 
  CountsSize_ATLeast(operation, counts, 2);
  bool hasFloat = false;
  for (Node count : counts) {
    IsNumber(operation, count);
    if (count->type == Node_Type::Float) hasFloat = true;
  }

  if (operation == "+") {
    if (!hasFloat) {
      long long total = 0;
      for (Node count : counts) total += GetIntValue(count);
      return MakeNode(Node_Type::Int, to_string(total));
    }

    double total = 0.0;
    for (Node count : counts) total += GetDoubleValue(count);
    return MakeNode(Node_Type::Float, to_string(total));
  }

  if (operation == "-") {
    if (!hasFloat) {
      long long total = GetIntValue(counts[0]);
      for (int i = 1; i < (int)counts.size(); ++i) total -= GetIntValue(counts[i]);
      return MakeNode(Node_Type::Int, to_string(total));
    }

    double total = GetDoubleValue(counts[0]);
    for (int i = 1; i < (int)counts.size(); ++i) total -= GetDoubleValue(counts[i]);
    return MakeNode(Node_Type::Float, to_string(total));
  }

  if (operation == "*") {
    if (!hasFloat) {
      long long total = 1;
      for (Node count : counts) total *= GetIntValue(count);
      return MakeNode(Node_Type::Int, to_string(total));
    }

    double total = 1.0;
    for (Node count : counts) total *= GetDoubleValue(count);
    return MakeNode(Node_Type::Float, to_string(total));
  }

  //operation == "/"

  for (int i = 1; i < (int)counts.size(); ++i) {
    if ((counts[i]->type == Node_Type::Int && GetIntValue(counts[i]) == 0) ||
        (counts[i]->type == Node_Type::Float && fabs(GetDoubleValue(counts[i])) < 1e-12)) {  
          //判断除数是否为0,fabs()函数返回一个数的绝对值
          //less than 1e-12 means it's close enough to zero to be considered zero
      throw MakeEvalError(EvalError_Type::DivisionByZero, "/", NULL);
    }
  }
  
  if (!hasFloat) {
    long long total = GetIntValue(counts[0]);
    for (int i = 1; i < (int)counts.size(); ++i) total /= GetIntValue(counts[i]);
    return MakeNode(Node_Type::Int, to_string(total));
  }

  double total = GetDoubleValue(counts[0]);
  for (int i = 1; i < (int)counts.size(); ++i) total /= GetDoubleValue(counts[i]);
  return MakeNode(Node_Type::Float, to_string(total));
}

Node GO_Not( vector<Node> &counts) {
  Counts_size("not", counts, 1);
  return MakeBooleanNode(IsFalseValue(counts[0]));
}

Node GO_NumericCompare(string name,  vector<Node> &counts) {
  CountsSize_ATLeast(name, counts, 2);
  for (Node count : counts) IsNumber(name, count);

  for (int i = 0; i + 1 < (int)counts.size(); ++i) {
    double left = GetDoubleValue(counts[i]);
    double right = GetDoubleValue(counts[i + 1]);
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

Node GO_StringAppend( vector<Node> &counts) {
  CountsSize_ATLeast("string-append", counts, 2);
  string result;
  for (Node count : counts) {
    Type_String("string-append", count);
    result += count->text;
  }

  return MakeNode(Node_Type::String, result);
}

Node GO_StringCompare(string name,  vector<Node> &counts) {
  CountsSize_ATLeast(name, counts, 2);
  for (Node count : counts) Type_String(name, count);

  for (int i = 0; i + 1 < (int)counts.size(); ++i) {
    string left = counts[i]->text;
    string right = counts[i + 1]->text;
    bool ok = false;

    if (name == "string>?") ok = left > right;
    else if (name == "string<?") ok = left < right;
    else ok = left == right;

    if (!ok) return MakeNil();
  }

  return MakeTrue();
}

bool NodeEqv(Node a, Node b) {
  if (a == b) return true;
  if (a->type == Node_Type::String || b->type == Node_Type::String) return false;
  if (a->type == Node_Type::Cons || b->type == Node_Type::Cons) return false;

  if (IsNumber(a) && IsNumber(b)) {
    return fabs(GetDoubleValue(a) - GetDoubleValue(b)) < 1e-12;
  }

  if (a->type != b->type) return false;
  return a->text == b->text;
}

bool NodeEqual(Node a, Node b) {
  if (a == b) return true;
  if (IsNumber(a) && IsNumber(b)) {
    return fabs(GetDoubleValue(a) - GetDoubleValue(b)) < 1e-12;
  }

  if (a->type != b->type) return false;
  if (a->type == Node_Type::Cons) {
    return NodeEqual(a->left, b->left) && NodeEqual(a->right, b->right);
  }

  return a->text == b->text;
}

Node GO_Equality(string name,  vector<Node> &counts) {
  Counts_size(name, counts, 2);
  if (name == "eqv?") return MakeBooleanNode(NodeEqv(counts[0], counts[1]));
  return MakeBooleanNode(NodeEqual(counts[0], counts[1]));
}

Node GO_Procedure(string name,  vector<Node> &counts, Node whole, //GO_Procedure函数根据名字调用对应的函数，并传入参数列表counts和环境env
                       Environment &env) {
  if (name == "cons") return GO_Cons(counts);
  if (name == "list") return GO_List(counts);
  if (name == "car") return GO_Car(counts);
  if (name == "cdr") return GO_Cdr(counts);
  if (name == "atom?" || name == "pair?" || name == "list?" || name == "null?" ||
      name == "integer?" || name == "real?" || name == "number?" ||
      name == "string?" || name == "boolean?" || name == "symbol?") {
    return GO_Predicate(name, counts);
  }
  if (name == "+" || name == "-" || name == "*" || name == "/") {
    return GO_AddSubMulDiv(name, counts);
  }
  if (name == "not") return GO_Not(counts);
  if (name == ">" || name == ">=" || name == "<" || name == "<=" || name == "=") {
    return GO_NumericCompare(name, counts);
  }
  if (name == "string-append") return GO_StringAppend(counts);
  if (name == "string>?" || name == "string<?" || name == "string=?") {
    return GO_StringCompare(name, counts);
  }
  if (name == "eqv?" || name == "equal?") return GO_Equality(name, counts);
  if (name == "clean-environment") {
    Counts_size("clean-environment", counts, 0);
    env.ClearUserDefinitions();
    return MakeMessage("environment cleaned");
  }
  if (name == "exit") {
    Counts_size("exit", counts, 0);
    return MakeProcedure("exit");
  }

  throw MakeEvalError(EvalError_Type::Calling_something_not_function_as_function,
                      AtomToString(MakeProcedure(name)), whole);
}

Node Eval(Node node, Environment &env, bool topLevel) {
  if (node->type == Node_Type::Nil || node->type == Node_Type::True ||
      node->type == Node_Type::Int || node->type == Node_Type::Float ||
      node->type == Node_Type::String || node->type == Node_Type::Procedure ||
      node->type == Node_Type::Message) {//如果是原子节点，直接返回
    return node;
  }

  if (node->type == Node_Type::Symbol) {
    return EvalSymbol(node, env);
  }

  EnsureProperListCall(node);
  vector<Node> items = ListToVector(node);
  if (items.empty()) return MakeNil();

  if (items[0]->type == Node_Type::Symbol) {
    string name = items[0]->text;
    if (name == "define" && !topLevel) {
      throw MakeEvalError(EvalError_Type::LevelOfDefine, "", node);
    }
    if (name == "quote") return EvalQuote(items, node);
    if (name == "define") return EvalDefine(items, node, env);
    if (name == "if") return EvalIf(items, node, env);
    if (name == "cond") return EvalCond(items, node, env);
    if (name == "begin") return EvalBegin(items, node, env);
    if (name == "and") return EvalAnd(items, node, env);
    if (name == "or") return EvalOr(items, node, env);
  }

  Node procedure = Eval(items[0], env, false);
  if (procedure->type != Node_Type::Procedure) {
    throw MakeEvalError(EvalError_Type::Calling_something_not_function_as_function,
                        AtomToString(procedure), procedure);
  }

  Valid(procedure->text, (int)items.size() - 1, node, topLevel);

  vector<Node> counts;
  for (int i = 1; i < (int)items.size(); ++i) {
    counts.push_back(Eval(items[i], env, false));
  }

  return GO_Procedure(procedure->text, counts, node, env);
}

void PrintEvalError(EvalError error) {//Eval錯誤的類型有很多種，根據不同的類型打印不同的錯誤信息
  if (error.type == EvalError_Type::NonList) {
    cout << "ERROR (non-list) : ";
    PrintSExp(error.node);
    
  } else if (error.type == EvalError_Type::Wrong_NumberOfArguments) {
    cout << "ERROR (incorrect number of arguments) : " << error.name << endl;

  } else if (error.type == EvalError_Type::Wrong_ArgumentType) {
    cout << "ERROR (" << error.name << " with incorrect argument type) : ";
    if (IsAtom(error.node)) cout << AtomToString(error.node) << endl;
     else PrintSExp(error.node);
    

  } else if (error.type == EvalError_Type::Calling_something_not_function_as_function) {
    cout << "ERROR (attempt to apply non-function) : ";
    if (IsAtom(error.node)) cout << AtomToString(error.node) << endl;
    else PrintSExp(error.node);
    
  } else if (error.type == EvalError_Type::NoReturnValue) {
    cout << "ERROR (no return value) : ";
    PrintSExp(error.node);

  } else if (error.type == EvalError_Type::UnboundSymbol) {
    cout << "ERROR (unbound symbol) : " << error.name << endl;

  } else if (error.type == EvalError_Type::DivisionByZero) {
    cout << "ERROR (division by zero) : /" << endl;

  } else if (error.type == EvalError_Type::DefineFormat) {
    cout << "ERROR (DEFINE format) : ";
    PrintSExp(error.node);

  } else if (error.type == EvalError_Type::LevelOfDefine) {
    cout << "ERROR (level of DEFINE)" << endl;
    
  } else if (error.type == EvalError_Type::LevelOfCleanEnvironment) {
    cout << "ERROR (level of CLEAN-ENVIRONMENT)" << endl;

  } else if (error.type == EvalError_Type::LevelOfExit) {
    cout << "ERROR (level of EXIT)" << endl;

  } else {
    cout << "ERROR (COND format) : ";
    PrintSExp(error.node);
  }
}

int main() {
  string Input;
  getline(cin, Input);

  cout << "Welcome to OurScheme!" << endl << endl;

  Parser parser;
  Environment env;
  bool endedByExit = false;

  while (true) {
    cout << "> ";
    Parser::Result result = parser.Read();

    if (result.cleanEof) {
      Print_token_error_message({TokenError_Type::No_More_Input, "", 0, 0});
      break;
    }

    if (!result.success) {
      Print_token_error_message(result.error); //tokenText is used in some error messages
      if (result.error.type == TokenError_Type::No_More_Input) {
        break;
      }

      cout << endl;
      continue;
    }

    if (IsExit(result.expression)) {
      endedByExit = true;
      break;
    }

    try {
      Node value = Eval(result.expression, env, true); 
      PrintSExp(value);
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
