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

# Examples
```lua
local hpi = $lua(math.pi/2) -- sets hpi to half of pi, this division will happen at compile time
local letters = {$lua(
    local result = {}
    for byte=string.byte"A",string.byte"Z" do -- assuming ASCII
        table.insert(result,string.char(byte).."=0,")
    end
    return result
)} -- creates a table with each uppercase letter initialized with the value zero
```

# What the preprocessor does
The preprocessor fundementally performs token manipulation: a sequence of tokens comes in, and a sequence of tokens is outputted. Here, a token refers to a lexical element such as a string literal or name. The supported syntax for tokens in the preprocessor is equivalent to Lua with a few additions:
* String literals may contain unescaped end of line sequences, they will be interpreted as if they were escaped.
* The escape sequence `\s` will result in a space character, this is intended to stop a `\z` escape sequnce while adding a space.
* Binary and octal literals are supported, which work very similarly to hexadecimal literals (even allowing floating point constants in binary and octal).
* Underscore digit separators are allowed in all positions after the first digit, e.g. `123_456.789_123` or the extreme case `1_2_._3_4_e_+_5_`. Underscores cannot be placed prior to a digit, preventing numbers such as `_123_456` and `._123` that would be ambiguous with names.
* The characters ``@!`?$`` are allowed outside of string literals as single character symbols.

These extensions are only present when the preprocessor is scanning for tokens, the standard Lua compiler will still be the same. When converting tokens into a string representation, numeric constants and strings literals will be written in a format that is valid in standard Lua.

The preprocessor converts the input file to tokens, scanning left to right and doing macro expansions. The result is written to the output file.

Each symbol has an associated amount of 'not nows', which indicates that a symbol has no special meaning. These can be directly specified by putting one or more `\` characters before a symbol, each backslash adds one notnow to the symbol. Space characters will be ignored, e.g. `\ \ $` is allowed. This is intended to suppress macro expansion, or to use unbalanced brackets for functions that use bracketed token sequences. In a context where a symbol always has a special meaning, the number of 'not nows' must be zero. Each time a symbol with notnows is checked for special meaning, a 'not now' will be removed. A symbol shouldn't have 'not nows' when converting it into a string representation.

Macro expansions are done using a `$` symbol with no 'not nows'. After the `$` symbol, one or more string literals or names will be scanned for, delimited by periods. The first name will be used to index the macros table, the result of which should be a function, built in macro, or table to search further into. If a table is found, then another name will be scanned for and this process repeats but with the previously found table. Scanning will stop when the final string literal or name is found that finds the function or built in macro to call. While scanning, macro expansions will be done, for example: `$$lua("lua")()` is valid.

Once the built in macro or function macro is found to call, it will be invoked with a reference to the preprocessor state and the number of tables searched through (other than the macros table) to find the macro to invoke. Function macros are provided the reference to the preprocessor state that allows manipulating the tokens, getting the macros table, setting the macros table to a new table, and setting error information. Tokens prior to the `$` symbol are not visible, and cannot be changed nor viewed. A reference to a preprocessor state should only be used when macro expansion with a function is occuring, unless otherwise specified (e.g. you cannot interact with the preprocessor state while a macro is being searched inside of __index).

When the function macro returns, the visible tokens are the result of macro expansion. Generally, a macro should only try to view and manipulate tokens in a defined region, such as in a set of brackets rather than work with all visible tokens.

Built in macros should only be invoked with their original name, and should only be invoked directly by putting them in the macros table. The default preprocessing state will start with a macros table containg just the built in macros: `now`, `notnow`, `totokens`, `tostring`, `concat`, `if`, `defined`, `lua`, `none`. Here, 'brackets' is used to refer to parentheses, square brackets, or curly brackets. All types of brackets are treated equally, and when counting brackets to find the ending bracket they will be treated equally. Built in macros will scan left to right and do macro expansions, unless stated othwerwise.

# The built in macros
* `none`: Removes the symbol `$` and the name `none`.
```lua
$none -- Expands to nothing
```
* `lua`: Evaluates an expression or statements enclosed with brackets after the name `lua`. A reference to the preprocessor state is provided in the `...` arguments. References to the preprocessor state can be used, and only the tokens after the closing bracket are visible. The symbol `$`, the name `lua`, and the expression or statements enclosed with brackets are replaced by the return value. The return value should be:
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
* `defined`: Expects one or more strings or names separated by periods after the name `defined`, searching just like it was trying to invoke the macro using the path. The symbol `$`, the name `defined`, and the path will be replaced by `true` if a function or built in macro located correctly was found, or `false` otherwise.
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
* `if`: Expects an if branch, then zero or more elseif or else branches, and finally a terminating end. The if and elseif branches are followed by a bracketed condition and a bracketed branch, both of which may be prefixed with a `::` symbol. The else branches are followed by a bracketed branch, which may be prefixed with `::` symbol. Branches are evaluated left to right. The first if or elseif branch that has a condition of true or else branch is the selected branch, and will be the branch used for getting the result. When evaluating a condition, if a correct branch hasn't been reached yet then it will expect to find only true or false as names or string literals in the condition. If a correct branch has been reached then any bracketed sequence of tokens is valid, and any `$` symbols without 'not nows' will not be evaluated past the opening bracket unless the `::` symbol prefix is used. Bracketed branches other than the selected branch (if any) without a symbol prefix `::` will be evaluated without evaluating any `$` symbols without 'not nows' past the opening bracket. The symbol `$`, the name `end`, and all tokens in between except the tokens inside of the selected bracketed branch (if any) will be removed.
```lua
$if(true){1}else{2}end -- 1
$if(false){1}else{2}end -- 2
$if(false){3}end -- nothing
$if(true){}else{$lua(error())}end -- nothing, no error because the second $ is ignored here
$if(true){}else::{$lua(error())}end -- error, the $ is evaluated anyway because of the ::
$if(false){}elseif(true){1}elseif($lua(error())){}end -- 1, no error because the $ is ignored here (and :: can be used again to force evaluation)
-- branches after an else branch are allowed:
$if(true){}else{}elseif(){}else{}end -- nothing, and the elseif doesn't need anything in the condition when not being checked
```
* `concat`: Concatenates one or more strings, or one or more names, terminated by a semicolon. The symbol `$`, the symbol `;`, and all tokens in between are replaced by the combined string or name.
```lua
$concat a b c; -- abc
$concat "a" "b" "c"; -- "abc"
$concat a; -- a
$concat a "b"; -- error, cannot mix strings and names
```
* `tostring`: Converts a bracketed sequence of tokens that will be converted into a string. The specific format shouldn't be relied upon, tokens may have multiple ways of being converted into the string, and the way spaces are inserted shouldn't be relied upon. The symbol `$`, the name `tostring`, and the bracketed sequence of tokens are replaced by the string reperesentation of the tokens.
```lua
$tostring(1+2) -- "0X1+0X2"
$tostring(()) -- "()"
$tostring() -- ""
$tostring($concat a b c;) -- "abc"
$tostring(\$concat a b c;) -- "$concat a b c;"
```
* `totokens`: Converts a string to tokens, doing the opposite function of `tostring`. The symbol `$`, the name `totokens`, and the string are replaced by the tokens read from the string.
```lua
$totokens"abc" -- abc
$totokens"(1+2)" -- (1+2)
$totokens"$lua(1+2)" -- 3, the $ in the resulting tokens gets expanded after totokens finishes
$totokens$tostring(a b c) -- a b c, tostring and totokens do opposite things
```
* `notnow`: Adds 'not nows' to tokens. The brackets in this description are used to indicate optionality, `[number]` means a number may or may not appear. `number` refers to a nonnegative integer, or a floating pointer number that is exactly representable as a nonnegative integer. This macro is overloaded to do multiple things:
    * `notnow [number] ;`: Adds the specified number of 'not nows' to the symbol `$`, or one if no number is specified. The name `notnow`, the number, and the symbol `;` will be removed.
    * `notnow [number] : symbol`: Adds the specified number of 'not now's to the symbol, or one if no number is specified. The symbol `$`, the name `notnow`, the number, and the symbol `:` will be removed.
    * `notnow [number] [::] bracketed token sequence`: Adds the specified number of 'not nows' to the symbols in the bracketed token sequence, or one if no number is specified. If the symbol `::` is not specified, any `$` symbols without 'not nows' will not be evaluated after the opening bracket. The symbol `$`, the name `notnow`, the number, the symbol `::`, and the beginning and ending brackets will be removed.
    * `notnow [number] ? [::] bracketed token sequence`: Moves the tokens inside of the bracketed token sequence to the end of the visible tokens. Any `$` symbols without 'not nows' will not be evaluated after the opening bracket if the symbol `::` is not specified. After moving the tokens, it will scan those tokens, then add the specified number of 'not nows' to the symbols, or one if no number is specified. The remaining tokens are placed back, and the symbol `$`, the name `notnow`, the number, the symbol `?`, the symbol `::`, and the beginning and ending brackets will be removed.
```lua
$notnow;none -- $none, same as \$ here
$tostring($notnow:]) -- "]", same as \] here, this is interesting if the token used is created from another macro
$notnow($lua(1+2)) -- $lua(1+2), just a convient way of adding 'not nows' to some tokens
$notnow::($lua(foo())) -- similar to above, but doing macro expansions so the result depends upon foo
$tostring($notnow?($totokens"(")) -- a simple way of adding 'not nows' to the results of macro expansion
```
* `now`: Expects a bracketed sequence of tokens. The symbol `$`, the name `now`, and the beginning and ending bracket will be removed. This macro is intended to do an extra evaluation to remove 'not nows'.
```lua
$now(\$)none -- nothing
$now(\$lua(1)) -- 1
```

# The preprocessor interface
A preprocessor state is created by the preprocessor, and references to this preprocessor state are provided when invoking macros or through the built in macro `lua`. These references allow manipulating the preprocessor state, methods are provided that will change the preprocessor state or the tokens it contains. The preprocessor state contains a cursor, which is used to identify the current token being viewed. This cursor will be saved when invoking a macro from within a macro. The cursor can be in the invalid state, indicating that no token is being viewed. When a macro is invoked, the cursor will by default be set to the first visible token, or the invalid state if there are no visible tokens.

A global function `tokens` is provided that will create a new preprocessor state, and return a reference to it. The macros table is provided with the first argument. By default, this preprocessor state will contain no tokens. When no macro is being invoked on this preprocessor state, all tokens are visible and the references to the preprocessor state can be used.
```lua
-- an example of the tokens function and some of the preprocessor interface
local x = $lua(
    local p = ... -- get the reference from the arguments
    local t = tokens(p:get_macros()) -- create a new tokens list, same macros
    t:insert_at_start() -- insert a token at the start
    t:set_type"symbol" -- make it a symbol
    -- by default, the symbol will be a $
    t:insert_ahead() -- add another token
    t:set_type"name" -- make it a name
    t:set_content"totokens" -- specifically the name 'totokens'
    t:insert_ahead() -- and another token
    t:set_type"string" -- that is a string
    t:set_content"y" -- "y"
    t:go_to_start() -- now go back to looking at the $
    t:handle_dollar() -- invoke this macro using all the tokens we have set up
    p:copy(t) -- we know this string will result in 1 token
    -- so we can copy over this token after $lua
) 1
```

Errors can occur during preprocessing, each preprocessing state can be in an error state. When the preprocessor state enters the error state, a string is stored containing the error information. Information will be added about the currently invoked macros as the error is returning back. The only methods accessible during an error are the functions for getting or setting an error. Errors are not recoverable within a preprocessing state, but can be handled from outside of the preprocessing state. Some possible reasons for an error to occur are: failing to find a built in macro or function to invoke, using a built in macro incorrectly, setting an error with the preprocessor interface, a function macro erroring, or a memory error.

Incorrectly using the preprocessor interface will result in the method to generate an error, but no error will be set on the preprocessor state.

# The methods of the preprocessor interface
All method names use `snake_case`. When a cursor's token is mentioned the cursor must not be in the invalid state, unless stated otherwise.

`get_content()` and `set_content(content)` will get or set (respectively) the content of the cursor's token. For strings and names, a string with the contents will be used. For integers and floats, integers and floats will be used. For symbols, their string representations will be used.

`get_type()` and `set_type(type)` will get or set (respectively) the type of the cursor's token. The types are: `string`, `name`, `integer`, `float`, and `symbol`. `set_type` will set the content to a default value: the empty string for strings, `nil` for names, zero for integers and floats, and `$` for symbols.

`get_not_now_amount()` and `set_not_now_amount(nonnegative_integer)` will get or set (respectively) the amount of 'not nows' on the cursor's token. The cursor's token must be a symbol.

`is_valid()` will return `false` if the cursor is in the invalid state, and `true` otherwise.

`make_invalid()` will put the cursor in the invalid state.

`is_advancing_valid()` and `is_retreating_valid()` will return `false` if advancing or retreating (respectively) will leave the the cursor in the invalid state, and `true` othwerise. The cursor must not be in the invalid state.

`go_to_start()` and `go_to_end()` will go the the start or end (respectively), or end up in the invalid state if there are no visible tokens.

`advance()` and `retreat()` will advance or retreat (respectively), or end up in the invalid state if there are no visible tokens after or before (respectively) the cursor's token.

`remove_and_advance()` and `remove_and_retreat()` will remove the cursor's token and advance or retreat (respectively), or end up in the invalid state if there are no visible tokens after or before (respectively) the cursor's token.

`insert_at_start()` and `insert_at_end()` will insert a new token at the start or end (respectively), and set the cursor to point at the new token. The token will start as an integer with the value zero.

`insert_ahead()` and `insert_behind()` will insert a new token ahead or behind (respectively) of the cursor's token, and set the cursor to point at the new token. The token will start as an integer with the value zero.

`insert_at_start_and_stay()`, `insert_at_end_and_stay()`, `insert_ahead_and_stay()`, and `insert_behind_and_stay()` are the same as the methods without the `_and_stay` suffix, but the cursor will not change.

`steal_to_start_and_advance(other_preprocessing_state)`, `steal_to_end_and_advance(other_preprocessing_state)`, `steal_ahead_and_advance(other_preprocessing_state)`, `steal_behind_and_advance(other_preprocessing_state)`, `steal_to_start_and_retreat(other_preprocessing_state)`, `steal_to_end_and_retreat(other_preprocessing_state)`, `steal_ahead_and_retreat(other_preprocessing_state)`, and `steal_behind_and_retreat(other_preprocessing_state)`: The cursor's token is taken from the other preprocessing state, and moved into the provided preprocessing state (the self argument). The token is moved to the location specified by the first part of the name (for `ahead` and `advance` the cursor must not be in the invalid state), and the cursor is set to point at this token. The cursor of the other preprocessing state will be advanced or retreated according to the last part of the name similar to `advance()` and `retreat()`. The other preprocessing state must not be the same as the provided preprocessing state, and must be in a state where the preprocessing interface can be used with it.

`handle_dollar()` will do a macro expansion. The cursor's token must be a `$` symbol without any 'not nows'. The cursor will point at the first token of the macro expansion, or will be set in the invalid state if the macro expansion is empty. If an error is set during macro expansion, this function will raise an error.

`handle_dollar_and_not_nows()` will do macro expansion like `handle_dollar()` zero or more times until the cursor's token is not a `$` symbol without any 'not nows'. The cursor will point at the first token of the last macro expansion, or will be set to the invalid state if the macro expansion was empty. If the last macro expansion was not empty and the first token of the macro expansion is a symbol with one or more 'not nows', one 'not now' will be removed and `true` will be returned. In all other cases, `false` will be returned.

`copy(other_preprocessing_state)` will copy the type, contents, and 'not nows' (for symbols) of the provided preprocessing state's cursor's token into the cursor's token (of the preprocessing state which the method was used on). The other preprocessing state must be in a state where the preprocessing interface can be used with it.

`shift_to_start()`, `shift_to_start_and_advance()`, `shift_to_start_and_retreat()`, `shift_to_end()`, `shift_to_end_and_advance()`, `shift_to_end_and_retreat()`: Shifts the cursor's token to the start or end as it says in the name. The cursor will follow the token as it shifts for `shift_to_start()` and `shift_to_end()`, otherwise it will advance or retreat as it says in the name similar to `advance()` and `retreat()` (the new cursor is decided before shifting).

`swap_with_start()`, `swap_with_end()`, `swap_ahead()`, `swap_behind()`: Swaps the type, content, and 'not nows' (for symbols) of the cursor's token with the token specified in the name. For `swap_ahead()` and `swap_behind()`, there must be tokens ahead of and behind (respectively) the cursor's token. For `swap_with_start()` and `swap_with_end()`, if the cursor's token is the starting or ending token (respectively) then nothing will happen.

`swap_between(other_preprocessing_state)`: Swaps the type, content, and 'not nows' (for symbols) of the cursor's token and the cursor's token of the other preprocessing state. The other preprocessing state must be in a state where the preprocessing interface can be used with it.

`get_macros()` and `set_macros(table)` will get or set (respectively) the macros table.

`get_error()` will get the current error message contained in the preprocessing state, or `nil` if no error exists. This method can always be used, even when the rest of the macro API cannot.

`set_error(string)` will set the error message contained in the preprocessing state. This function will not raise an error in the calling Lua code assuming no memory errors occur, if this type of quick exiting is desired then an error should be created explicitly.

`clear()` will remove every visible token, and set the cursor in the invalid state.
