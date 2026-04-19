#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum class Token_Type {
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
  No_Closing_Quote  // a string forgot its last ".
};

enum class Node_Type {
  Nil,
  True,
  Int,
  Float,
  String,
  Symbol,
  Cons
};

enum class Error_Type {
  UnexpectedAtom_Or_LeftParen, 
  Unexpected_RightParen,
  No_Closing_Quote,
  No_More_Input
};

struct Token {
  Token_Type type;
  string input;
  string output;
  int line;
  int column;
};

struct Node {
  Node_Type type;
  string text;
  Node *left;
  Node *right;
};

struct error_message {
  Error_Type type;
  string tokenText;
  int line;
  int column;
};

struct line_column {
  int line;
  int column;
};

using NodePtr = Node*;

NodePtr MakeNode(Node_Type type, string text = "") {
  NodePtr node(new Node());
  node->type = type;
  node->text = text;
  node->left = NULL;
  node->right = NULL;
  return node;
}

NodePtr MakeNil() {
  return MakeNode(Node_Type::Nil, "nil");
}

NodePtr MakeTrue() {
  return MakeNode(Node_Type::True, "#t");
}

NodePtr MakeCons(NodePtr left, NodePtr right) {
  NodePtr node = MakeNode(Node_Type::Cons);
  node->left = left;
  node->right = right;
  return node;
}

NodePtr MakeAtomFromToken(Token token) {
  if (token.type == Token_Type::Nil) return MakeNil();
  if (token.type == Token_Type::True) return MakeTrue();
  if (token.type == Token_Type::Int) return MakeNode(Node_Type::Int, token.input);
  if (token.type == Token_Type::Float) return MakeNode(Node_Type::Float, token.input);
  if (token.type == Token_Type::String) return MakeNode(Node_Type::String, token.input);
  return MakeNode(Node_Type::Symbol, token.input);
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


bool IsInt(string text) {  
  if (text.empty()) return false;

  int index = 0;
  if (text[index] == '+' || text[index] == '-') { //+123  -456
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

//=======================================================================
Token Sort_TokenType(string text, int line, int column) {
  Token token;
  token.input = text;  //the original text of the token
  token.output = text;  //final output text
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
  string Line;  //the current line being processed
  int index_line; //the line number of the current line, starting from 1
  int position; //the index of the next to read in Line
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
  Lexer()
  {
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
    while (true) {  //一直跳過 空白 / 註解 / 空行
      if ((!useful_line || position >= (int)Line.size()) && !ReadNextLine()) { //EOF
        return MakeQuickToken(Token_Type::EndOfFile, "", index_line + 1, 1);
      }

      while (position < (int)Line.size() &&
             isspace((unsigned char)Line[position])) {
        position++;
      }

      if (position >= (int)Line.size()) {  //空行
        continue;
      }

      if (Line[position] == ';') {    //dfghgf;  because ';' starts a comment, we can skip to the end of the line.
        position = (int)Line.size();  // Skip the rest of the line as it's a comment.
        continue;
      }

      break;
    }

    // Now we are at the start of the next token.
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

        if (current == '"') { //String token ends when we see the next '"'
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
            value += current;
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
  bool peeked = false;

  Token PeekToken() {
    if (!peeked) {
      peek = lexer.NextToken(); //如果之前沒有 peek 過，就從 lexer 讀取下一個 token。
      peeked = true;
    }

    return peek;
  }

  Token Use_Token() {
    Token token = PeekToken();  //使用 peek 的 token 作為當前 token，並將 peeked 設為 false，表示下次需要從 lexer 讀取新的 token。
    peeked = false;
    return token;
  }

  error_message Right_error_message(error_message error, line_column start) {
    if (error.type == Error_Type::No_More_Input) {
      return error;
    }

    error_message result = error;
    result.line = error.line - start.line + 1;

    if (result.line <= 1) {
      result.line = 1;
      result.column = error.column - start.column + 1;
    }

    if (result.column < 1) {
      result.column = 1;
    }

    return result;
  }

  error_message MakeUnexpectedAtom_Or_LeftParen(Token token) {
    error_message error;
    error.type = Error_Type::UnexpectedAtom_Or_LeftParen;
    error.tokenText = token.output;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  error_message MakeUnexpected_RightParen(Token token) {
    error_message error;
    error.type = Error_Type::Unexpected_RightParen;
    error.tokenText = token.output;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  error_message MakeNo_Closing_Quote(Token token) {
    error_message error;
    error.type = Error_Type::No_Closing_Quote;
    error.line = token.line;
    error.column = token.column;
    return error;
  }

  error_message MakeNo_More_Input() {
    error_message error;
    error.type = Error_Type::No_More_Input;
    error.line = 0;
    error.column = 0;
    return error;
  } 

  bool IsAtomToken(Token_Type type) {
    return type == Token_Type::Int || type == Token_Type::Float ||
           type == Token_Type::String || type == Token_Type::Nil ||
           type == Token_Type::True || type == Token_Type::Symbol;
  }

  NodePtr MakeQuoteNode(NodePtr quoted) {
    vector<NodePtr> items;
    items.push_back(MakeNode(Node_Type::Symbol, "quote"));
    items.push_back(quoted);
    return BuildList(items, MakeNil());
  }

  bool ParseSExp(NodePtr &result, error_message &error) {
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
      NodePtr quoted;
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

      vector<NodePtr> items;
      NodePtr first;
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
          NodePtr tail;
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

        NodePtr item;
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
    NodePtr expression;
    error_message error;
  };



  Result Read() {
    Result result;
    result.success = false;
    result.cleanEof = false;

    line_column start = lexer.FindStartPlace(); //下一個 token 的位置
    Token first = PeekToken();

    if (first.type == Token_Type::EndOfFile) {
      result.cleanEof = true;
      return result;
    }

    if (first.type == Token_Type::No_Closing_Quote) {
      result.error = Right_error_message(MakeNo_Closing_Quote(first), start);
      lexer.DiscardRestOfLine(first.line);
      peeked = false;
      return result;
    }

    //start 
    error_message error;
    NodePtr expression;
    if (!ParseSExp(expression, error)) {
      result.error = Right_error_message(error, start);
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

bool IsAtom(NodePtr node) {
  return node->type != Node_Type::Cons;
}


string AtomToString(NodePtr node) {
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
  return node->text;
}
vector<string> PrettyPrint(NodePtr node);

void AddSpace(NodePtr child, vector<string> &lines) {
  vector<string> childLines = PrettyPrint(child);
  lines.push_back("  " + childLines[0]);
  for (size_t i = 1; i < childLines.size(); ++i) {
    lines.push_back("  " + childLines[i]);
  }
}

vector<string> PrettyPrint(NodePtr node) {
  if (IsAtom(node)) { // If this is an atom, just return its string representation as the only line.
    return { AtomToString(node) };
  }

  vector<string> lines;

  vector<NodePtr> items;
  NodePtr current = node;
  while (current->type == Node_Type::Cons) {
    items.push_back(current->left);
    current = current->right;
  }
  NodePtr tail = current;  // This is the tail. For a normal list, it is nil.


  // Put the first item right after "( ".
  vector<string> firstLines = PrettyPrint(items[0]);
  lines.push_back("( " + firstLines[0]);
  for (size_t i = 1; i < firstLines.size(); ++i) {
    lines.push_back("  " + firstLines[i]);
  }

  // Put each next item on a new line.
  for (size_t i = 1; i < items.size(); ++i) {
    AddSpace(items[i], lines);
  }

  // If this is a dotted pair, print the dot and the tail.
  if (tail->type != Node_Type::Nil) {
    lines.push_back("  .");
    AddSpace(tail, lines);
  }

  lines.push_back(")");
  return lines;
}


void PrintSExp(NodePtr node) {
  vector<string> lines = PrettyPrint(node);
  for (string &line : lines) {
    cout << line << "\n"; // Print each line with the given indent.
  }
}

void DeleteTree(NodePtr node) {
  if (node == NULL) return;

  DeleteTree(node->left);
  DeleteTree(node->right);
  delete node;
}

bool IsExit(NodePtr node) {  // Check (exit)
  if (node->type != Node_Type::Cons) return false;
  if (node->left->type != Node_Type::Symbol || node->left->text != "exit") return false;
  return node->right->type == Node_Type::Nil;
}

void Print_error_message(error_message error) {
  if (error.type == Error_Type::UnexpectedAtom_Or_LeftParen) {
    cout << "ERROR (unexpected token) : atom or '(' expected when token at Line "
         << error.line << " Column " << error.column << " is >>"
         << error.tokenText << "<<" << endl;

  } else if (error.type == Error_Type::Unexpected_RightParen) {
    cout << "ERROR (unexpected token) : ')' expected when token at Line "
         << error.line << " Column " << error.column << " is >>"
         << error.tokenText << "<<" << endl;
         
  } else if (error.type == Error_Type::No_Closing_Quote) {
    cout << "ERROR (no closing quote) : END-OF-LINE encountered at Line "
         << error.line << " Column " << error.column << endl;

  } else {
    cout << "ERROR (no more input) : END-OF-FILE encountered" << endl;
  }

}

int main() {
  string Input;
  getline(cin, Input);

  cout << "Welcome to OurScheme!" << endl << endl;

  Parser parser;
  
  bool endedByExit = false;
  while (true) {
    cout << "> ";
    Parser::Result result = parser.Read();

    if (result.cleanEof) {
      Print_error_message({Error_Type::No_More_Input, "", 0, 0});
      break;
    }

    if (!result.success) {
      Print_error_message(result.error);
      if (result.error.type == Error_Type::No_More_Input) {
        break;
      }
      cout << endl;
      continue;
    }

    if (IsExit(result.expression)) {
      DeleteTree(result.expression);
      endedByExit = true;
      break;
    }

    PrintSExp(result.expression);
    cout << endl;
    DeleteTree(result.expression);
  }

  if (endedByExit) {
    cout << endl;
    cout << "Thanks for using OurScheme!";
  } else {
    cout << "Thanks for using OurScheme!";
  }

  return 0;
}
