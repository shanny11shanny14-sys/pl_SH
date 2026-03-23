#include <iostream>
#include <string>
#include <map>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <iomanip>

using namespace std;

const double EPS = 0.0001;

enum TokenType {
  TOK_ID,
  TOK_NUM,
  TOK_ASSIGN,   // :=
  TOK_PLUS,     // +
  TOK_MINUS,    // -
  TOK_MUL,      // *
  TOK_DIV,      // /
  TOK_LPAREN,   // (
  TOK_RPAREN,   // )
  TOK_SEMI,     // ;
  TOK_EQ,       // =
  TOK_NEQ,      // <>
  TOK_GT,       // >
  TOK_LT,       // <
  TOK_GEQ,      // >=
  TOK_LEQ,      // <=
  TOK_END,
  TOK_BAD
};

struct Token {
  TokenType type;
  string text;
};

struct Value {
  bool isFloat;
  double num;
};

map<string, Value> vars; // Store all variables and their values.

int testNum;
string line = "";
int pos = 0;
bool eof = false;

Token currentToken;
Token peekToken; // Place the token we peeked
bool hasPeek = false; // have the token we peeked or not
bool hasCurrent = false;
bool hasError = false;

enum ErrorKind {
  ErrorUnrecognized,  // Unrecognized token error
  ErrorUnexpected,    // Unexpected token error
  ErrorUndefined,     // Undefined identifier error
  ErrorOther          // Other error, like divide by zero
};

struct ParseError {
  ErrorKind kind;
  string text;
};

ParseError errorInfo;

bool IsInteger(double x) {
  double r = round(x);  //round(3.2) -> 3
  return fabs(x - r) < EPS;
}

void PrintValue(Value v) {
  if (v.isFloat) {
    cout << fixed << setprecision(3) << v.num << endl;
  } else {
    cout << (long long)round(v.num) << endl;
  }
}

void PrintBool(bool b) {
  if (b) cout << "true" << endl;
  else cout << "false" << endl;
}

bool ReadNextLine() {
  if (!getline(cin, line)) {
    eof = true;
    return false;
  }

  line += '\n';
  pos = 0;
  return true;
}

char PeekChar() {
  while (true) {
    if (pos < (int)line.size()) {
      return line[pos];
    }

    if (!ReadNextLine()) {
      return '\0';
    }
  }
}

char GetChar() {
  char ch = PeekChar();
  if (ch != '\0') {
    pos++;
  }

  return ch;
}

void SkipSpaceAndComment() {
  while (true) {
    char ch = PeekChar();

    while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
      GetChar();
      ch = PeekChar();
    }

    if (ch == '/') {
      int oldPos = pos;
      GetChar();
      char nextCh = PeekChar();
      pos = oldPos;

      if (nextCh == '/') {
        while (true) {
          char c = GetChar();
          if (c == '\0' || c == '\n') {
            break;
          }
        }
        continue;
      }
    }

    break;
  }
}

Token GetToken() {
  SkipSpaceAndComment();

  char ch = PeekChar();

  if (ch == '\0') {
    return {TOK_END, ""};
  }

  if (isalpha(ch)) {
    string s;
    while (isalnum(PeekChar()) || PeekChar() == '_') {
      s += GetChar();
    }

    return {TOK_ID, s};
  }

  if (isdigit(ch) || ch == '.') {
    string s;
    bool hasDot = false;

    if (ch == '.') {
      hasDot = true;
      s += GetChar();

      if (!isdigit(PeekChar())) {
        return {TOK_BAD, "."};
      }
    }

    while (isdigit(PeekChar()) || PeekChar() == '.') {
      char c = PeekChar();
      if (c == '.') {
        if (hasDot) {
          break;
        }
        hasDot = true;
      }
      s += GetChar();
    }

    return {TOK_NUM, s};
  }

  if (ch == ':') {
    GetChar();
    if (PeekChar() == '=') {
      GetChar();
      return {TOK_ASSIGN, ":="};
    }
    return {TOK_BAD, ":"};
  }

  if (ch == '+') {
    GetChar();
    return {TOK_PLUS, "+"};
  }
  if (ch == '-') {
    GetChar();
    return {TOK_MINUS, "-"};
  }
  if (ch == '*') {
    GetChar();
    return {TOK_MUL, "*"};
  }
  if (ch == '/') {
    GetChar();
    return {TOK_DIV, "/"};
  }
  if (ch == '(') {
    GetChar();
    return {TOK_LPAREN, "("};
  }
  if (ch == ')') {
    GetChar();
    return {TOK_RPAREN, ")"};
  }
  if (ch == ';') {
    GetChar();
    return {TOK_SEMI, ";"};
  }
  if (ch == '=') {
    GetChar();
    return {TOK_EQ, "="};
  }

  if (ch == '<') {
    GetChar();
    if (PeekChar() == '>') {
      GetChar();
      return {TOK_NEQ, "<>"};
    }
    if (PeekChar() == '=') {
      GetChar();
      return {TOK_LEQ, "<="};
    }
    return {TOK_LT, "<"};
  }

  if (ch == '>') {
    GetChar();
    if (PeekChar() == '=') {
      GetChar();
      return {TOK_GEQ, ">="};
    }
    return {TOK_GT, ">"};
  }

  string bad;
  bad += GetChar();
  return {TOK_BAD, bad};
}

Token TakeRawToken() {
  if (hasPeek) {
    hasPeek = false;
    return peekToken;
  }
  return GetToken();
}

ParseError MakeUnrecognized(string s) {
  ParseError e;
  e.kind = ErrorUnrecognized;
  e.text = s;
  return e;
}

ParseError MakeUnexpected(string s) {
  ParseError e;
  e.kind = ErrorUnexpected;
  e.text = s;
  return e;
}

ParseError MakeUndefined(string s) {
  ParseError e;
  e.kind = ErrorUndefined;
  e.text = s;
  return e;
}

ParseError MakeOther() {
  ParseError e;
  e.kind = ErrorOther;
  e.text = "";
  return e;
}

void SetError(ParseError e) {
  if (!hasError) {
    hasError = true;
    errorInfo = e;
  }
}

Value MakeDummyValue() {
  Value v;
  v.num = 0;
  v.isFloat = false;
  return v;
}

Token ReadCheckedToken() {
  Token t = TakeRawToken();
  if (t.type == TOK_BAD) {
    SetError(MakeUnrecognized(t.text));
    return {TOK_END, ""};
  }
  return t;
}

Token PeekCheckedToken() {
  if (!hasPeek) {
    peekToken = GetToken();
    hasPeek = true;
  }

  if (peekToken.type == TOK_BAD) {
    SetError(MakeUnrecognized(peekToken.text));
    return {TOK_END, ""};
  }

  return peekToken;
}

void Advance() {
  currentToken = ReadCheckedToken();
  hasCurrent = true;
}

bool IsBoolOp(TokenType t) {
  return t == TOK_EQ || t == TOK_NEQ || t == TOK_GT ||
         t == TOK_LT || t == TOK_GEQ || t == TOK_LEQ;
}

bool IsValidTokenAfterFirstID(TokenType t) {
  return t == TOK_ASSIGN ||
         t == TOK_PLUS || t == TOK_MINUS || t == TOK_MUL || t == TOK_DIV ||
         t == TOK_SEMI || IsBoolOp(t);
}

void SkipRestOfLineAfterError() {
  pos = (int)line.size();
  hasPeek = false;
  hasCurrent = false;
  hasError = false;
}

Value MakeNumValue(string s) {
  Value v;
  v.num = atof(s.c_str());
  v.isFloat = (s.find('.') != string::npos);
  return v;
}

Value AddValue(Value a, Value b) {
  Value v;
  v.num = a.num + b.num;
  v.isFloat = a.isFloat || b.isFloat;
  return v;
}

Value SubValue(Value a, Value b) {
  Value v;
  v.num = a.num - b.num;
  v.isFloat = a.isFloat || b.isFloat;
  return v;
}

Value MulValue(Value a, Value b) {
  Value v;
  v.num = a.num * b.num;
  v.isFloat = a.isFloat || b.isFloat;
  return v;
}

Value DivValue(Value a, Value b) {
  Value v;
  v.num = a.num / b.num;

  if (!a.isFloat && !b.isFloat) {
    long long leftInt = (long long)round(a.num);
    long long rightInt = (long long)round(b.num);
    v.isFloat = (leftInt % rightInt != 0);
  } else {
    v.isFloat = true;
  }

  return v;
}

Value Calculate_PLUS_MINUS();

Value ParseFactor() {
  if (hasError) return MakeDummyValue();

  if (currentToken.type == TOK_ID) {
    string name = currentToken.text;
    if (vars.find(name) == vars.end()) {
      SetError(MakeUndefined(name));
      return MakeDummyValue();
    }
    Value v = vars[name];
    Advance();
    return v;
  }

  if (currentToken.type == TOK_LPAREN) {
    Advance();
    Value v = Calculate_PLUS_MINUS();
    if (hasError) return MakeDummyValue();
    if (currentToken.type != TOK_RPAREN) {
      SetError(MakeUnexpected(currentToken.text));
      return MakeDummyValue();
    }
    Advance();
    return v;
  }

  if (currentToken.type == TOK_PLUS || currentToken.type == TOK_MINUS) {
    TokenType sign = currentToken.type;
    Advance();
    if (hasError) return MakeDummyValue();

    if (currentToken.type != TOK_NUM) {
      SetError(MakeUnexpected(currentToken.text));
      return MakeDummyValue();
    }

    Value v = MakeNumValue(currentToken.text);
    if (sign == TOK_MINUS) {
      v.num = -v.num;
    }
    Advance();
    return v;
  }

  if (currentToken.type == TOK_NUM) {
    Value v = MakeNumValue(currentToken.text);
    Advance();
    return v;
  }

  SetError(MakeUnexpected(currentToken.text));
  return MakeDummyValue();
}

Value Calculate_MUL_DIV() {
  Value left = ParseFactor();
  if (hasError) return MakeDummyValue();

  while (currentToken.type == TOK_MUL || currentToken.type == TOK_DIV) {
    TokenType op = currentToken.type;
    Advance();
    if (hasError) return MakeDummyValue();
    Value right = ParseFactor();
    if (hasError) return MakeDummyValue();

    if (op == TOK_MUL) {
      left = MulValue(left, right);
    } else {
      if (fabs(right.num) < EPS) {
        SetError(MakeOther());
        return MakeDummyValue();
      }
      left = DivValue(left, right);
    }
  }

  return left;
}

Value Calculate_PLUS_MINUS() {
  Value left = Calculate_MUL_DIV();
  if (hasError) return MakeDummyValue();

  while (currentToken.type == TOK_PLUS || currentToken.type == TOK_MINUS) {
    TokenType op = currentToken.type;
    Advance();
    if (hasError) return MakeDummyValue();
    Value right = Calculate_MUL_DIV();
    if (hasError) return MakeDummyValue();

    if (op == TOK_PLUS) {
      left = AddValue(left, right);
    } else {
      left = SubValue(left, right);
    }
  }

  return left;
}

bool CompareValue(Value a, Value b, TokenType op) {
  double diff = a.num - b.num;

  if (op == TOK_EQ) return fabs(diff) <= EPS;   // Small difference means equal.
  if (op == TOK_NEQ) return fabs(diff) > EPS;   // Big enough difference means not equal.
  if (op == TOK_GT) return diff > EPS;          // Must be really bigger.
  if (op == TOK_LT) return diff < -EPS;         // Must be really smaller.
  if (op == TOK_GEQ) return diff > -EPS;        // Bigger or almost same is OK.
  return diff < EPS;                            // Smaller or almost same is OK.
}

bool ParseCommand() {
  if (hasError) return false;

  if (currentToken.type == TOK_ID && currentToken.text == "quit") {
    return true;
  }

  if (currentToken.type == TOK_ID) {
    Token first = currentToken;
    Token next = PeekCheckedToken();
    if (hasError) return false;

    if (!IsValidTokenAfterFirstID(next.type)) {
      SetError(MakeUnexpected(next.text));
      return false;
    }

    if (next.type == TOK_ASSIGN) {
      string name = first.text;

      Advance(); // eat ':='
      if (hasError) return false;
      Advance(); // move to first token of arith exp
      Value v = Calculate_PLUS_MINUS();
      if (hasError) return false;

      if (currentToken.type != TOK_SEMI) {
        SetError(MakeUnexpected(currentToken.text));
        return false;
      }

      vars[name] = v;
      PrintValue(v);
      Advance(); // move to next command's first token
      return false;
    }

    if (vars.find(first.text) == vars.end()) {
      SetError(MakeUndefined(first.text));
      return false;
    }
  }

  Value left = Calculate_PLUS_MINUS();
  if (hasError) return false;

  if (IsBoolOp(currentToken.type)) {
    TokenType op = currentToken.type;
    Advance();
    if (hasError) return false;
    Value right = Calculate_PLUS_MINUS();
    if (hasError) return false;

    if (currentToken.type != TOK_SEMI) {
      SetError(MakeUnexpected(currentToken.text));
      return false;
    }

    PrintBool(CompareValue(left, right, op));
    Advance(); // move to next command's first token
    return false;
  }

  if (currentToken.type != TOK_SEMI) {
    SetError(MakeUnexpected(currentToken.text));
    return false;
  }

  PrintValue(left);
  Advance(); // move to next command's first token
  return false;
}

int main() {
  cin >> testNum;
  string dummy;
  getline(cin, dummy);

  cout << "Program starts..." << endl;

  while (true) {
    cout << "> ";
      if (!hasCurrent) {
        Advance();
      }

    bool shouldExit = ParseCommand();
    if (shouldExit) {
      break;
    }

    if (hasError) {
      if (errorInfo.kind == ErrorUnrecognized) {
        cout << "Unrecognized token with first char : '" << errorInfo.text << "'" << endl;
      } else if (errorInfo.kind == ErrorUnexpected) {
        cout << "Unexpected token : '" << errorInfo.text << "'" << endl;
      } else if (errorInfo.kind == ErrorUndefined) {
        cout << "Undefined identifier : '" << errorInfo.text << "'" << endl;
      } else {
        cout << "Error" << endl;
      }

      SkipRestOfLineAfterError();
    }
  }

  cout << "Program exits..." << endl;
  return 0;
}
