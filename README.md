# LuaPreprocessor
A preprocessor for Lua. The purpose of this is to provide compile time manipulation of the program, to avoid doing unnecessary work at runtime and to make it possible to simplify constructs that are repeated many times.

# Compiling
A shell file is included in src named Make.sh, it will invoke gcc with the provided arguments and all of the sources files needed to compile.
`./Make.sh -o Preprocessor` should be good enough to compile the project into an executable named `Preprocessor`. Any extra arguments can be provided, for example to add optimizations or to modify Lua.

If you wish to bring your own libraries into the preprocessor, this can be done by modifying `Libraries.c` to load your libraries.

# Usage
This information will be generated when invoking the preprocessor with no arguments. `./Preprocessor` will be replaced by the program name.
```
usage: ./Preprocessor input [output]
Available input options:
  -e in  use the argument 'in' as input
  -- f   use the file 'f' as input
  -      use the standard input as input
  f      use the file 'f' as input, if it doesn't begin with '-'
Available output options:
  f      use the file 'f' as output
         or use the standout output if no output is provided
```
Input is specified by a file name, stdin with `-`, or as a program argument with `-e`. Output is a file name, or stdout if nothing is provided. In the exceptional case where a file name begins with `-`, a special case is made for `--` to interpret the next argument as a file name. All files are opened in text mode.

Example: `./Preprocessor foo.lua out.lua` reads from `foo.lua` and writes to `out.lua`

# What the preprocessor does
The preprocessor fundementally performs token manipulation: a sequence of tokens comes in, and a sequence of tokens is outputted. Here, a token refers to a lexical element such as a string literal or name. The supported syntax for tokens in the preprocessor is equivalent to Lua with a few additions:
* String literals may contain unescaped end of line sequences, they will be interpreted as if they were escaped.
* The escape sequence `\s` will result in a space character, this is intended to stop a `\z` escape sequnce while adding a space.
* Binary and octal literals are supported, which work very similarly to hexadecimal literals (even allowing floating point constants in binary and octal).
* Underscore digit separators are allowed in all positions after the first digit, e.g. `123_456.789_123` or the extreme case `1_2_._3_4_e_+_5_`. Underscores cannot be placed prior to a digit, preventing numbers such as `_123_456` and `._123` that would be ambiguous with identifiers.
* The characters ``@!`?$`` are allowed outside of string literals as single character symbols.

These extensions are only present when the preprocessor is scanning for tokens, the standard Lua compiler will still be the same. When converting tokens into a string representation, numeric constants and strings literals will be written in a format that is valid in standard Lua.

The preprocessor converts the input file to tokens, scanning left to right and doing macro expansions. The result is written to the output file.

Each symbol has an associated amount of 'not nows', which indicates that a symbol has no special meaning. These can be directly specified by putting one or more `\` characters before a symbol, each backslash adds one notnow to the symbol. Space characters will be ignored, e.g. `\ \ $` is allowed. This is intended to suppress macro expansion, or to use unbalanced brackets for functions that use bracketed token sequences. In a context where a symbol always has a special meaning, the number of 'not nows' must be zero. Each time a symbol with notnows is checked for special meaning, a 'not now' will be removed. A symbol shouldn't have 'not nows' when converting it into a string representation.

Macro expansions are done using a `$` with no 'not nows'. After the `$`, one or more string literals or names will be scanned for, delimited by periods. The first name will be used to index the macros table, the result of which should be a function, built in macro, or table to search further into. If a table is found, then another name will be scanned for and this process repeats but with the previously found table. Scanning will stop when the final string literal or name is found that finds the function or built in macro to call. While scanning, macro expansions will be done, for example: `$$lua("lua")()` is valid.

Once the built in macro or function is found to call, it will be invoked with a reference to the preprocessor state and the number of tables searched through (other than the macros table) to find the macro to invoke. Functions are provided in the reference to the preprocessor state that allow manipulating the tokens, getting the macros table, setting the macros table to a new table, and setting error information. Tokens prior to the `$` are not visible, and cannot be changed nor viewed. A reference to a preprocessor state should only be used when macro expansion with a function is occuring, unless otherwise specified.

When the function returns, the visible tokens are the result of macro expansion. Generally, a macro should only try and view and manipulate tokens in a defined region, such as in a set of brackets rather than work with all visible tokens.

Built in macros should only be invoked with their original name, and should only be invoked directly by putting them in the macros table. The default preprocessing state will start with a macros table containg just the built in macros: `now`, `notnow`, `totokens`, `tostring`, `concat`, `if`, `defined`, `lua`, `none`. Here, 'brackets' is used to refer to parentheses, square brackets, or curly brackets. All types of brackets are treated equally, and when counting brackets to find the ending bracket they will be treated equally. Built in macros will scan left to right and do macro expansions, unless stated othwerwise.

Explanations and examples of the built in macros:
* `none`: Removes the `$` and the name `none`.
```lua
$none -- Expands to nothing
```
* `lua`: Evaluates an expression or statements enclosed with brackets after the name `lua`. A reference to the preprocessor state is provided in the `...` arguments. References to the preprocessor state can be used, and only the tokens after the closing bracket are visible. The `$`, the name `lua`, and the expression or statements are replaced by the return value. The return value should be:
    * nil, the value nil is the result.
    * A boolean, false or true is the result.
    * A number, this number is the result and must not be NaN. If the number is a negative floating point number, it will be replaced by four tokens `(-N)` where N is the number.
    * A string, this string is the result.
    * A table, values are read from the table like it is an array (starting at index 1, incrementing the index until it find the first nil value). Each value should be a string, which will be converted to tokens for the result. The tokens generated from each of the strings are combined for the result. These strings are evaluated separately, meaning tables such as `{"[[","]]"}` are not valid.
    * No values, if zero values are returned then the result is nothing.
    * If multiple values are returned, all values except the first are ignored.
```lua
$lua(1+2) -- 3
$lua() -- nothing
$lua(local string = "abc" return string) -- "abc"
$lua(math.abs(-1)) -- 1, this can be evaluated as an expression or a statement so it gets evaluated as an expression
$lua(math.abs(-1);) -- a statement
$lua({"local x","=1","local","y"}) -- local x=1 local y
-- $lua is executing Lua code at compile time, and can store variables in the globals table that are shared between uses of $lua
$lua(
    x = 1
    function foo(v)
        return v+1
    end
) -- nothing
$lua(foo(x)) -- 2, uses globals defined before
-- this is by far the most powerful built in macro
```
* `defined`: Expects one or more strings or names separated by periods after the name `defined`, searching just like it was trying to invoke the macro using the path. The `$`, the name `defined`, and the path will be replaced by `true` if a function or built in macro located correctly was found, or `false` otherwise.
```lua
$defined defined -- true
$lua(
    (...):get_macros().x = {
        y = function()end -- this is defined just for the example
    }
)
$defined x.y -- true
$defined x.z -- false
$defined random.y -- false.y, note that it stops scanning at 'random' so it never removes the .y
```
* `if`: TODO
* `concat`: TODO
* `tostring`: TODO
* `totokens`: TODO
* `notnow`: TODO
* `now`: TODO

# Examples
```lua
local hpi = $lua(math.pi/2) -- sets hpi to half of pi, this division is guaranteed to happen at compile time
```
