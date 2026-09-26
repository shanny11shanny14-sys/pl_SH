# OurScheme Interpreter

一個使用 **C++17** 實作的 OurScheme 直譯器，支援 S-expression 讀取、語法分析、pretty print、基本運算、變數綁定、條件判斷、區域變數、lambda、使用者自訂函式，以及 Project 4 的 I/O 與 eval 相關功能。

本專案是依照課程 OurScheme Project 1 ~ Project 4 逐步完成，重點在練習：

- Scanner / Lexer
- Parser
- S-expression tree structure
- Pretty printer
- Environment / symbol binding
- Evaluator
- Primitive procedures
- Special forms
- Function call and local scope

---

## Features

### Project 1：Read / Parse / Pretty Print

Project 1 主要負責把使用者輸入的文字轉成 OurScheme 的 S-expression 結構，並印出標準格式。

支援的 token 包含：

- `LEFT-PAREN`：`(`
- `RIGHT-PAREN`：`)`
- `INT`：例如 `123`, `+123`, `-123`
- `FLOAT`：例如 `123.456`, `123.`, `.456`, `-.123`
- `STRING`：例如 `"hello"`
- `DOT`：`.`
- `NIL`：`nil`, `#f`, `()`
- `T`：`t`, `#t`
- `QUOTE`：`'`
- `SYMBOL`：例如 `abc`, `+`, `hello-world`

支援的 S-expression 型態：

```scheme
123
3.14
"hello"
abc
()
(1 2 3)
(1 . 2)
'(1 2 3)
```

`'x` 會被視為：

```scheme
(quote x)
```

---

### Project 2：Evaluation and Primitive Procedures

Project 2 開始支援 evaluation，也就是不只印出 S-expression，而是會真正執行它。

支援的 primitive procedures：

#### Constructors

```scheme
(cons 1 2)
(list 1 2 3)
```

#### Quote

```scheme
(quote (1 2 3))
'(1 2 3)
```

#### Binding

```scheme
(define x 10)
x
```

#### List Accessors

```scheme
(car '(1 2 3))
(cdr '(1 2 3))
```

#### Predicates

```scheme
(atom? 1)
(pair? '(1 . 2))
(list? '(1 2 3))
(null? nil)
(integer? 3)
(real? 3.14)
(number? 3)
(string? "hi")
(boolean? #t)
(symbol? 'abc)
```

#### Arithmetic / Logic / String

```scheme
(+ 1 2)
(- 10 3)
(* 2 3)
(/ 10 2)
(not nil)
(and #t #t)
(or nil #t)
(> 3 2)
(>= 3 3)
(< 1 2)
(<= 2 2)
(= 3 3)
(string-append "hello" " world")
(string>? "b" "a")
(string<? "a" "b")
(string=? "hi" "hi")
```

#### Equality

```scheme
(eqv? 1 1)
(equal? '(1 2) '(1 2))
```

#### Sequencing and Conditionals

```scheme
(begin
  (define x 10)
  (+ x 5))

(if (> 3 2)
    "yes"
    "no")

(cond
  ((> 3 5) "wrong")
  ((< 3 5) "correct")
  (else "other"))
```

#### Environment Reset

```scheme
(clean-environment)
```

---

### Project 3：User-defined Functions, `let`, and `lambda`

Project 3 加入使用者自訂函式與區域變數。

#### Function Definition

```scheme
(define (square x)
  (* x x))

(square 5)
```

#### Lambda

```scheme
((lambda (x y)
   (+ x y))
 3 4)
```

#### Let

```scheme
(let ((x 3)
      (y 5))
  (+ x y))
```

`let` 會建立 local environment，變數只在 `let` body 裡有效。

#### Function Body

函式 body 可以有多個 S-expression，最後一個 expression 的值會成為函式回傳值。

```scheme
(define (test x)
  (+ x 1)
  (* x 2))

(test 5) ; 回傳 10
```

---

### Project 4：I/O, Error Object, Eval, and Set

Project 4 加入更多進階功能。

#### Error Object

```scheme
(create-error-object "something wrong")
(error-object? (create-error-object "error"))
```

#### Read / Write / Display

```scheme
(read)
(write '(1 2 3))
(display-string "hello")
(newline)
```

#### Conversion

```scheme
(symbol->string 'abc)
(number->string 123)
(number->string 3.14)
```

#### Eval

```scheme
(eval '(+ 1 2))
```

#### Set

```scheme
(set! x 20)
```

`set!` 可以修改已存在的 binding，也可以回傳被設定的新值。

---

## Project Structure

目前主要程式可以放成以下結構：

```text
OurScheme/
├── README.md
├── main.cpp
├── tests/
│   ├── project1.in
│   ├── project2.in
│   ├── project3.in
│   └── project4.in
└── docs/
    ├── OurSchemeIntro.pdf
    ├── OurSchemeProj1.pdf
    ├── OurSchemeProj2.pdf
    ├── OurSchemeProj3.pdf
    └── OurSchemeProj4.pdf
```

---

## Main Components

### 1. Lexer

負責從使用者輸入中切出 token。

主要工作：

- 跳過空白
- 處理註解 `;`
- 辨識 `(`、`)`、`.`、`'`
- 處理 string 與 escape character
- 判斷 token 是 int、float、symbol、string、nil、true

---

### 2. Parser

負責把 token 組成 S-expression tree。

支援：

- atom
- list
- dotted pair
- quote
- syntax error handling

---

### 3. Node

使用 `Node_Type` 表示不同資料型態：

```cpp
Nil
True
Int
Float
String
Error
Symbol
Cons
Procedure
Message
```

`Cons` node 使用 `left` 和 `right` 模擬 Scheme 的 cons cell。

---

### 4. Environment

負責處理 symbol binding。

支援：

- primitive procedure binding
- user-defined binding
- nested environment
- global definition
- local scope
- clean-environment

---

### 5. Evaluator

Evaluator 負責真正執行 S-expression。

特殊形式包含：

```scheme
quote
if
cond
begin
and
or
define
lambda
let
set!
```

普通 function call 會先 evaluate 參數，再執行對應 procedure。

---

## Build

使用 g++ 編譯：

```bash
g++ -std=c++17 -O2 -Wall main.cpp -o ourscheme
```

---

## Run

互動式執行：

```bash
./ourscheme
```

Windows PowerShell：

```powershell
.\ourscheme.exe
```

---

## Example

```text
Welcome to OurScheme!

> (define x 10)
x defined

> (+ x 5)
15

> (list 1 2 3)
( 1
  2
  3
)

> (if (> x 5) "big" "small")
"big"

> (exit)

Thanks for using OurScheme!
```

---

## Error Handling

本專案處理兩大類錯誤：

### Syntax Error

例如：

```scheme
(1 2 . 3 4)
```

可能輸出：

```text
ERROR (unexpected token) : ')' expected ...
```

### Evaluation Error

例如：

```scheme
(+ 1 "hi")
```

可能輸出：

```text
ERROR (+ with incorrect argument type) : "hi"
```

常見 evaluation error：

- unbound symbol
- incorrect number of arguments
- incorrect argument type
- attempt to apply non-function
- non-list
- no return value
- division by zero
- define format error
- let format error
- lambda format error
- cond format error

---

## Notes

- This is an educational interpreter for a course project.
- It is not a full Scheme implementation.
- Uppercase and lowercase symbols are different.
- `nil`, `#f`, and `()` are treated as false.
- `t` and `#t` are treated as true.
- Floating point output is formatted to three decimal places.
- The interpreter uses an internal cons-cell structure to represent lists and dotted pairs.

---

## Author

Computer Science course project implementation.

