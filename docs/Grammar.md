# HxHLanguage的顶层结构是主函数。

### HxHLanguage 语法 BNF 定义

#### 1. 基础结构

```bnf
<TODO>         ::= < "#" | "//" | "<--" > ε
<program>      ::= <statement_list>
<statement_list>::= <statement> | <statement> <statement_list>
<statement>    ::= <var_decl> | <const_decl> | <fun_decl> | <class_decl> | <if_stmt> | <repeat_stmt> | <return_stmt>

```

#### 2. 变量与常量

```bnf
<type>         ::= "int" | "整型" | "float" | "浮点型" | "char" | "字符型" | "str" | "string" | "字符串型" | <custom_type>
<var_decl>     ::= "var" ":" <identifier> [ ":" <type> ] ";" | "定义变量" ":" <identifier> [ "," "类型是" ":" <type> ] ";"
<const_decl>   ::= "con" ":" <identifier> [ ":" <type> ] ";" | "定义常量" ":" <identifier> [ "," "类型是" ":" <type> ] ";"

```

#### 3. 函数定义

```bnf
<fun_decl>     ::= "fun" ":" <identifier> "(" <arg_list> ")" [ ":" <type> ] "->" <block> 
                 | "函数" ":" <identifier> "(" <arg_list> ")" [ "," "返回类型是" ":" <type> ] "->" <block>
<arg_list>     ::= <identifier> ":" <type> | <arg_list> "," <identifier> ":" <type> | ε
<block>        ::= "{" <statement_list> "}"

```

#### 4. 类与对象

```bnf
<class_decl>   ::= "cls" ":" [ <identifier> "->" ] <identifier> <block>
                 | "定义类" ":" <identifier> [ "," "父类是" ":" <identifier> ] "->" <block>
<access_mod>   ::= "public:" | "公有成员:" | "private:" | "私有成员:" | "protected:" | "受保护成员:"
<class_member> ::= <access_mod> <statement_list>

```

#### 5. 控制流

```bnf
<if_stmt>      ::= "if" ":" <expression> "->" <block> | "若" ":" <expression> "->" <block>
<repeat_stmt>  ::= "repeat" "->" <block> [ "until" "(" <expression> ")" ] 
                 | "循环" "->" <block> [ "直到" ":" <expression> ]
<return_stmt>  ::= "ret" ":" <expression> ";" | "返回" ":" <expression> ";"|"。"

```

#### 6. 表达式

```bnf
<args>         ::= {expression,}
<expression>   ::= <term> | <expression> <operator> <term>
<operator>     ::= "+" | "-" | "*" | "/" | "="
<term>         ::= <identifier> | <literal> | <fun_call>
<fun_call>     ::= <identifier> "(" [args] ")"

```

---

### 人话版
---
HxHLanguage的每条语句末尾都要有"。"或"；"或";"

## 1. 基础数据类型

语言内置了常见的数据类型，并且每种类型都支持中英文写法：

* **整数**：`int` 或 `整型`
* **浮点数**：`float` 或 `浮点型`
* **字符**：`char` 或 `字符型`
* **字符串**：`str`、`string` 或 `字符串型`、`字符串`
* **布尔值**：`bool` 或 `布尔型`
* **空类型**：`void` 或 `无返回类型`、`无参数`

**修饰符(尚未实现)**：

* **数组**：在类型后加 `[]` 即可声明数组（如 `int[]`）。
* **引用**：在类型后加 `&` 即可声明引用（如 `int&`），直接操作内存地址。

## 2. 变量与常量定义

在定义变量或常量时，语句必须以分号（`;`）结尾。标识符推荐使用 lowerCamelCase（小驼峰）命名法。

### 英文风格语法

* **变量**：`var : myVariable [ : myType ] ;`
* **常量**：`con : myConstant [ : myType ] ;`

### 中文风格语法

* **变量**：`定义变量 : myVariable [ , 类型是 : myType ] ;`
* **常量**：`定义常量 : myConstant [ , 类型是 : myType ] ;`

## 3. 函数定义

函数定义包含了参数列表和返回类型，函数体内可以包含多条语句。主函数（main 或 主函数）不能被重载。

### 英文风格语法

使用 `fun` 关键字声明，使用 `->` 连接函数体：

```text
fun : myFunction ( myArg : int ) : int -> {
    ret : 1 ;
}

```

*注：HxLanguage实现了返回类型推导。*

### 中文风格语法

使用 `函数` 声明：

```text
函数 : myFunction ( myArg : 整型 ) , 返回类型是 : 整型 -> {
    返回 : 1 ;
}

```

## 4. 类与对象 (面向对象(部分实现))

类可以包含成员变量和成员函数，并且支持继承机制。如果你定义的类互相包含（比如 A 包含 B，B 又包含 A），编译器会为了防止内存溢出而直接报错。

### 类的声明

* **英文风格**：`cls : [parentClass ->] myClass { ... }` （箭头前面的是父类）
* **中文风格**：`定义类 : myClass [ , 父类是 : parentClass ] -> { ... }`

### 访问权限修饰符

在类体（`{}`）内部，通过修饰符外加冒号（`:`）来划分权限区域：

* **公开**：`public :` 或 `公有成员 :`
* **私有**：`private :` 或 `私有成员 :`
* **保护**：`protected :` 或 `受保护成员 :`

## 5. 控制流 (条件与循环)

### 条件判断(部分实现) (If)

使用 `->` 指向条件成立时执行的语句或代码块：

* **英文**：`if : expression -> { ... }`
* **中文**：`若 : expression -> { ... }`

### 循环语句(部分实现) (Repeat)

用来重复执行一段代码块：

* **英文**：`repeat -> { ... } until：expression`
* **中文**：`循环 -> { ... } 直到 : expression`

## 6. 特殊内置操作

* **打印字符串**：底层预留了 `__hx_write_string__ "你好" ;` 这样的内置指令，用于直接向标准输出流写入字符。
* **返回值**：函数中返回数据使用 `ret : expression ;` 或 `返回 : expression ;`。

---