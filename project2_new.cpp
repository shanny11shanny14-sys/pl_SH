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
Node EvalEvalEval(Node node, Environment & env_setting, bool topLayer = false);
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
  return isspace( ch) || ch == '(' || ch == ')' ||
         ch == '\'' || ch == '"' || ch == ';';
}

bool IsInt(string text) {
  if (text.empty()) return false;

  int index = 0;
  if (text[index] == '+' || text[index] == '-') {
    index++;
  }

  if (index >= (int)text.size()) return false;

  for (int i = index; i < (int)text.size(); ++i) {
    if (!isdigit( text[i])) {
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
    } else if (isdigit( ch)) {
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
  else token.type = Token_Type::Symbol;//abc 

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
    index_line = 0; //index_line 是目前讀到的行數
    //position 是目前在這行的哪個位置
    position = 0;
    useful_line = false;    //useful_line 表示目前這行是否有用(如果這行是空行或者只有註解就沒有用)
    end = false;    //end 表示是否已經讀到檔案結尾

  }

  line_column FindStartPlace() {
    if (useful_line) {
      int probe = position;
      while (probe < (int)Line.size() &&
             isspace( Line[probe])) {
        probe++;
      }

      if (probe >= (int)Line.size() || Line[probe] == ';') {
        return {index_line + 1, 1};
      }

      return {index_line, position + 1}; //????
      /*debug: 這裡應該是 probe + 1 而不是 position + 1,
        因為 position 是指向第一個非空白字元的下一個位置,
        而 probe 是指向第一個非空白字元的位置
      */
    }

    return {index_line + 1, 1};
  }

  Token NextToken() {
    while (true) {//ReadNextLine讀取下一行
      if ((!useful_line || position >= (int)Line.size()) && !ReadNextLine()) {
        return MakeQuickToken(Token_Type::EndOfFile, "", index_line + 1, 1);
      }

      while (position < (int)Line.size() &&
             isspace( Line[position])) {
        position++;
      }

      if (position >= (int)Line.size()) {//如果這行沒有東西了,就繼續讀下一行
        continue;
      }

      if (Line[position] == ';') {//如果這行剩下的東西是註解了,就繼續讀下一行
        position = (int)Line.size();
        continue;
      }

      break; //如果這行還有東西可以讀,就跳出 while 迴圈,準備讀 token
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

        if (current == '\\' && position < (int)Line.size()) {//"hello \"world\""
          //遇到一個 '\' 就要看下一個字是什麼,如果是 n 就換成換行符號,如果是 t 就換成 tab 符號
          //如果是 " 就換成 ",如果是 \ 就換成 \,其他的就把 '\' 和下一個字都加到 value 裡面
          char next = Line[position++];
          if (next == 'n') value += '\n';
          else if (next == 't') value += '\t';
          else if (next == '"') value += '"';//意思是如果用户输入了 \" 就表示他们想在字符串里输入一个 " 字符,所以我们要把它转换成一个 " 字符
          else if (next == '\\') value += '\\';//如果是 '\' 就換成 '\',注意這裡要加兩個 '\' 才能表示一個 '\' 字元
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
    while (position < (int)Line.size()) {//'abc  abc 是 一個 token,當我遇到空白或者分隔符號就表示這個 token 結束了
      char current = Line[position];
      if (isprint( current) == 0|| IsSeparator(current)) { //當我遇到不能印出的字元就break,當我遇到分隔符號也break
        break;
      }

      text += current;
      position++;
    }

    return Sort_TokenType(text, startLine, startColumn);//
  }

  void Trash(int line) {
    if (useful_line && index_line == line) 
      position = (int)Line.size();
  }
};

class Parser {
 private:
  Lexer lexer;
  Token peek;
  bool peeked;
  //是否使用過 peek 的 token 了,如果 peeked 是 false 就表示 peek 裡面的 token 還沒有被使用過

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
    normalized.line = error.line - start.line + 1;//

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
    //return true 代表成功組成文法樹, result 就是組成的文法樹; return false 代表失敗, error 就是失敗的錯誤訊息
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
      if (!ParseSExp(first, error)) {//如果不是 ()，那 list 裡面一定至少有一個元素
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
          result = BuildList(items, tail);//dotted pair
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
    Node expression; //如果 success 是 true, expression 就是成功解析出来的表达式;如果 success 是 false, error 就是解析失败的错误信息
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
      lexer.Trash(first.line);
      peeked = false;
      return result;
    }

    token_error_message error;
    Node expression;
    if (!ParseSExp(expression, error)) {
      result.error = Right_token_error_message(error, start);
      if (error.line > 0) {
        lexer.Trash(error.line);
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

bool IsNormalList(Node node) {
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
  if (node->type == Node_Type::Int) return (double)stoll(node->text);

  return stod(node->text);//把字符串转换成双精度浮点数
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

void NewLine(Node child, vector<string> &lines) {
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
    NewLine(items[i], lines);
  }

  //如果 tail 不是 nil, 就在倒数第二行加上 " .", 然后把 tail 也放在新的一行
  if (tail->type != Node_Type::Nil) {
    lines.push_back("  .");
    NewLine(tail, lines);
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
  if (!IsNormalList(node)) return false;

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
  map<string, Node> original_setting;
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
      original_setting[name] = MakeProcedure(name);
    }
  }

  Node Search(string name) {
    if (user_defined.count(name)) return user_defined[name];
    if (original_setting.count(name)) return original_setting[name];
    return NULL;
  }

  bool IsPrimitiveName(string name) {
    return original_setting.count(name) > 0;
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


Node EvalSymbol(Node node, Environment & env_setting) {//把符号转换成它的值,如果符号没有绑定值,就报错
  Node value = env_setting.Search(node->text);
  if (value == NULL) {
    throw MakeEvalError(EvalError_Type::UnboundSymbol, node->text, node);
  }

  return value;
}


Node BooleanNode(bool value) {
  return value ? MakeTrue() : MakeNil();
}

Node EvalSequence( vector<Node> &items, int startIndex, Environment & env_setting) {
  Node result = MakeNil();
  for (int i = startIndex; i < (int)items.size(); ++i) {
    result = EvalEvalEval(items[i], env_setting, false);
  }

  return result;
}

Node EvalQuote( vector<Node> &items, Node whole) {
  if ((int)items.size() != 2) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "quote", whole);
  }

  return items[1];
}

Node EvalDefine( vector<Node> &items, Node whole, Environment & env_setting) {
  if ((int)items.size() != 3 || items[1]->type != Node_Type::Symbol ||
      env_setting.IsPrimitiveName(items[1]->text)) {
    throw MakeEvalError(EvalError_Type::DefineFormat, "", whole);
  }

  Node value = EvalEvalEval(items[2], env_setting, false);
  env_setting.Define(items[1]->text, value);
  return MakeMessage(items[1]->text + " defined");
}

Node EvalIf( vector<Node> &items, Node whole, Environment & env_setting) {
  if ((int)items.size() != 3 && (int)items.size() != 4) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "if", whole);
  }

  Node test = EvalEvalEval(items[1], env_setting, false);
  if (!IsFalseValue(test)) {
    return EvalEvalEval(items[2], env_setting, false);
  }

  if ((int)items.size() == 4) {
    return EvalEvalEval(items[3], env_setting, false);
  }

  throw MakeEvalError(EvalError_Type::NoReturnValue, "", whole);
}

Node EvalBegin( vector<Node> &items, Node whole, Environment & env_setting) {
  if ((int)items.size() < 2) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "begin", whole);
  }

  return EvalSequence(items, 1, env_setting);
}

Node EvalAnd( vector<Node> &items, Node whole, Environment & env_setting) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "and", whole);
  }

  Node result = MakeTrue();
  for (int i = 1; i < (int)items.size(); ++i) {
    result = EvalEvalEval(items[i], env_setting, false);
    if (IsFalseValue(result)) {
      return result;
    }
  }

  return result;
}

Node EvalOr( vector<Node> &items, Node whole, Environment & env_setting) {
  if ((int)items.size() < 3) {
    throw MakeEvalError(EvalError_Type::Wrong_NumberOfArguments, "or", whole);
  }

  Node result = MakeNil();
  for (int i = 1; i < (int)items.size(); ++i) {
    result = EvalEvalEval(items[i], env_setting);
    if (!IsFalseValue(result)) {
      return result;
    }
  }

  return result;
}

Node EvalCond( vector<Node> &items, Node whole, Environment & env_setting) {
  if ((int)items.size() < 2) {
    throw MakeEvalError(EvalError_Type::CondFormat, "", whole);
  }

  for (int i = 1; i < (int)items.size(); ++i) {
    Node IF = items[i];  //條件區塊
    if (IF->type == Node_Type::Nil || !IsNormalList(IF)) {
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
    else testValue = EvalEvalEval(parts[0], env_setting, false);

    if (!IsFalseValue(testValue))  return EvalSequence(parts, 1, env_setting);
    
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


void Valid(string name, int countCount, Node whole, bool topLayer) {
  if (name == "clean-environment" && !topLayer) 
    throw MakeEvalError(EvalError_Type::LevelOfCleanEnvironment, "", whole);
  
  if (name == "exit" && !topLayer) 
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

Node Belong_Cons( vector<Node> &counts) {
  Counts_size("cons", counts, 2);
  return MakeCons(counts[0], counts[1]);
}

Node Belong_List( vector<Node> &counts) {
  return BuildList(counts, MakeNil());
}

Node Belong_Car( vector<Node> &counts) {
  Counts_size("car", counts, 1);
  if (counts[0]->type != Node_Type::Cons) {
    throw MakeEvalError(EvalError_Type::Wrong_ArgumentType, "car", counts[0]);
  }

  return counts[0]->left;
}

Node Belong_Cdr( vector<Node> &counts) {
  Counts_size("cdr", counts, 1);
  if (counts[0]->type != Node_Type::Cons) {
    throw MakeEvalError(EvalError_Type::Wrong_ArgumentType, "cdr", counts[0]);
  }

  return counts[0]->right;
}

Node Belong_Predicate(string name,  vector<Node> &counts) {
  Counts_size(name, counts, 1);
  Node value = counts[0];

  if (name == "atom?") return BooleanNode(IsAtom(value));
  if (name == "pair?") return BooleanNode(value->type == Node_Type::Cons);
  if (name == "list?") return BooleanNode(value->type == Node_Type::Nil || IsNormalList(value));
  if (name == "null?") return BooleanNode(value->type == Node_Type::Nil);
  if (name == "integer?") return BooleanNode(value->type == Node_Type::Int);
  if (name == "real?" || name == "number?") return BooleanNode(IsNumber(value));
  if (name == "string?") return BooleanNode(value->type == Node_Type::String);
  if (name == "boolean?") return BooleanNode(value->type == Node_Type::Nil || value->type == Node_Type::True);
  return BooleanNode(value->type == Node_Type::Symbol);
}

Node Belong_AddSubMulDiv(string operation,  vector<Node> &counts) { 
  CountsSize_ATLeast(operation, counts, 2);
  bool hasFloat = false;
  for (Node count : counts) {
    IsNumber(operation, count);
    if (count->type == Node_Type::Float) hasFloat = true;
  }

  if (operation == "+") {
    if (!hasFloat) {
      long long total = 0;
      for (Node count : counts) total += stoll(count->text);
      return MakeNode(Node_Type::Int, to_string(total));
    }

    double total = 0.0;
    for (Node count : counts) total += GetDoubleValue(count);
    return MakeNode(Node_Type::Float, to_string(total));
  }

  if (operation == "-") {
    if (!hasFloat) {
      long long total = stoll(counts[0]->text);
      for (int i = 1; i < (int)counts.size(); ++i) total -= stoll(counts[i]->text);
      return MakeNode(Node_Type::Int, to_string(total));
    }

    double total = GetDoubleValue(counts[0]);
    for (int i = 1; i < (int)counts.size(); ++i) total -= GetDoubleValue(counts[i]);
    return MakeNode(Node_Type::Float, to_string(total));
  }

  if (operation == "*") {
    if (!hasFloat) {
      long long total = 1;
      for (Node count : counts) total *= stoll(count->text);
      return MakeNode(Node_Type::Int, to_string(total));
    }

    double total = 1.0;
    for (Node count : counts) total *= GetDoubleValue(count);
    return MakeNode(Node_Type::Float, to_string(total));
  }

  //operation == "/"

  for (int i = 1; i < (int)counts.size(); ++i) {
    if ((counts[i]->type == Node_Type::Int && stoll(counts[i]->text) == 0) ||
        (counts[i]->type == Node_Type::Float && fabs(GetDoubleValue(counts[i])) <  0.0001)) {  
          //判断除数是否为0,fabs()函数返回一个数的绝对值
          //less than 0 means it's close enough to zero to be considered zero
      throw MakeEvalError(EvalError_Type::DivisionByZero, "/", NULL);
    }
  }
  
  if (!hasFloat) {
    long long total = stoll(counts[0]->text);
    for (int i = 1; i < (int)counts.size(); ++i) total /= stoll(counts[i]->text);
    return MakeNode(Node_Type::Int, to_string(total));
  }

  double total = GetDoubleValue(counts[0]);
  for (int i = 1; i < (int)counts.size(); ++i) total /= GetDoubleValue(counts[i]);
  return MakeNode(Node_Type::Float, to_string(total));
}

Node Belong_Not( vector<Node> &counts) {
  Counts_size("not", counts, 1);
  return BooleanNode(IsFalseValue(counts[0]));
}

Node Belong_NumCompare(string name,  vector<Node> &counts) {//
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
    else ok = fabs(left - right) <  0.0001;

    if (!ok) return MakeNil();
  }

  return MakeTrue();
}

Node Belong_StringAppend( vector<Node> &counts) {
  CountsSize_ATLeast("string-append", counts, 2);
  string result;
  for (Node count : counts) {
    Type_String("string-append", count);
    result += count->text;
  }

  return MakeNode(Node_Type::String, result);
}

Node Belong_StringCompare(string name,  vector<Node> &counts) {
  CountsSize_ATLeast(name, counts, 2);
  for (Node count : counts) Type_String(name, count);

  for (int i = 0; i + 1 < (int)counts.size(); ++i) {
    string left = counts[i]->text;
    string right = counts[i + 1]->text;
    bool ok = false;

    if (name == "string>?") ok = left > right;  //choose one comparer based on the name
    else if (name == "string<?") ok = left < right;
    else ok = left == right;

    if (!ok) return MakeNil();
  }

  return MakeTrue();
}

bool NodeEqv(Node a, Node b) {//比較數值是不是一樣,如果是字符串或者 cons 就直接 false
  if (a == b) return true;

  if (a->type == Node_Type::String || b->type == Node_Type::String ||
      a->type == Node_Type::Cons || b->type == Node_Type::Cons) return false;
  
  if (IsNumber(a) && IsNumber(b)) return fabs(GetDoubleValue(a) - GetDoubleValue(b)) <  0.0001;

  if (a->type != b->type) return false;
  return a->text == b->text;
}

bool NodeEqual(Node a, Node b) {
  if (a == b) return true;
  if (IsNumber(a) && IsNumber(b)) {
    return fabs(GetDoubleValue(a) - GetDoubleValue(b)) <  0.0001; //fabs()函数返回一个数的绝对值,0.0001是因為浮點數的誤差
  }

  if (a->type != b->type) return false;
  if (a->type == Node_Type::Cons) {
    return NodeEqual(a->left, b->left) && NodeEqual(a->right, b->right);
  }

  return a->text == b->text;
}

Node Belong_Equality(string name, vector<Node> &counts) {
  Counts_size(name, counts, 2);
  if (name == "eqv?") return BooleanNode(NodeEqv(counts[0], counts[1]));
  return BooleanNode(NodeEqual(counts[0], counts[1]));
}

Node Which_Procedure(string name,  vector<Node> &counts, Node whole, 
                       Environment & env_setting) { //根据 procedure 的名字来判断是哪个函数,然后把参数传入對應的函數計算結果
  if (name == "cons") return Belong_Cons(counts);
  if (name == "list") return Belong_List(counts);
  if (name == "car") return Belong_Car(counts);
  if (name == "cdr") return Belong_Cdr(counts);
  if (name == "atom?" || name == "pair?" || name == "list?" || name == "null?" ||
      name == "integer?" || name == "real?" || name == "number?" ||
      name == "string?" || name == "boolean?" || name == "symbol?") {
    return Belong_Predicate(name, counts);
  }
  if (name == "+" || name == "-" || name == "*" || name == "/")  
    return Belong_AddSubMulDiv(name, counts);
  
  if (name == "not") return Belong_Not(counts);
  if (name == ">" || name == ">=" || name == "<" || name == "<=" || name == "=") 
    return Belong_NumCompare(name, counts);
  
  if (name == "string-append") return Belong_StringAppend(counts);
  if (name == "string>?" || name == "string<?" || name == "string=?") 
    return Belong_StringCompare(name, counts);
  
  if (name == "eqv?" || name == "equal?") return Belong_Equality(name, counts);

  if (name == "clean-environment") {
    Counts_size("clean-environment", counts, 0);
    env_setting.ClearUserDefinitions();
    return MakeMessage("environment cleaned");
  }

  if (name == "exit") {
    Counts_size("exit", counts, 0);
    return MakeProcedure("exit");
  }

  throw MakeEvalError(EvalError_Type::Calling_something_not_function_as_function,
                      AtomToString(MakeProcedure(name)), whole);
}

Node EvalEvalEval(Node node, Environment & env_setting, bool topLayer) {
   //如果是原子节点，直接返回
  if (node->type == Node_Type::Nil || node->type == Node_Type::True ||
      node->type == Node_Type::Int || node->type == Node_Type::Float ||
      node->type == Node_Type::String || node->type == Node_Type::Procedure ||
      node->type == Node_Type::Message) return node; //如果是原子节点，直接返回,因为原子节点本身就是值,不需要再计算了

  //只有atom是符号才需要查环境变量 ex: (define x 10) 然後的 (+ x 5) x 就是一个符号,需要查环境变量才能得到它的值
  if (node->type == Node_Type::Symbol)  return EvalSymbol(node, env_setting);
  
  if (!IsNormalList(node)) throw MakeEvalError(EvalError_Type::NonList, "", node);
  
  vector<Node> items = ListToVector(node); //把列表转换成vector
  if (items.empty()) return MakeNil();

  //如果列表的第一个元素是符号,就根据符号的名字来判断是哪个特殊形式
  if (items[0]->type == Node_Type::Symbol) { 
    string name = items[0]->text;
    if (name == "define" && !topLayer) throw MakeEvalError(EvalError_Type::LevelOfDefine, "", node);
    
    if (name == "quote") return EvalQuote(items, node);
    if (name == "define") return EvalDefine(items, node, env_setting);
    if (name == "if") return EvalIf(items, node, env_setting);
    if (name == "cond") return EvalCond(items, node, env_setting);
    if (name == "begin") return EvalBegin(items, node, env_setting);
    if (name == "and") return EvalAnd(items, node, env_setting);
    if (name == "or") return EvalOr(items, node, env_setting);
  }
//不是 quote / define / if / cond / begin / and / or 這些特殊形式，就往下跑
  Node procedure = EvalEvalEval(items[0], env_setting, false);//return the value of the first element in the list, which should be a procedure
  if (procedure->type != Node_Type::Procedure) //如果列表的第一个元素不是Procedure就报错
    throw MakeEvalError(EvalError_Type::Calling_something_not_function_as_function, AtomToString(procedure), procedure);

  Valid(procedure->text, (int)items.size() - 1, node, topLayer);//根据 procedure 的名字和参数的数量来判断是否合法,如果不合法就报错
  //格式正確否則不會丟錯誤,繼續往下跑
//==============================================================================
  vector<Node> Evalcounts; //Evalcounts 用來放「已經計算完成的參數」
  for (int i = 1; i < (int)items.size(); ++i) {
    //把参数(items)都计算出来後,放在 Evalcounts 里
    Evalcounts.push_back(EvalEvalEval(items[i], env_setting, false));
  }
  //根据 procedure 的名字来判断是哪个函数,然后把参数传进去计算结果
  return Which_Procedure(procedure->text, Evalcounts, node, env_setting); 
  //real calculation happens in Which_Procedure
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
  Environment env_setting;
  bool Exit = false;

  while (true) {
    cout << "> ";
    Parser::Result result = parser.Read();

    if (result.cleanEof) {
      Print_token_error_message({TokenError_Type::No_More_Input, "", 0, 0});
      break;
    }

    if (!result.success) {
      Print_token_error_message(result.error); //tokenText is used in some error messages
      if (result.error.type == TokenError_Type::No_More_Input) break;
      cout << endl;
      continue;
    }

    if (IsExit(result.expression)) {
      Exit = true;
      break;
    }

    try {
      Node value = EvalEvalEval(result.expression, env_setting, true); 
      PrintSExp(value);

    } catch (EvalError &error) {
      PrintEvalError(error);
    }
    cout << endl;
  }

  if (Exit) cout << endl;

  cout << "Thanks for using OurScheme!";
  return 0;
}
