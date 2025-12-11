# LuaPreprocessor
A preprocessor for Lua. The purpose of this is to provide compile time manipulation of the program, to avoid doing unnecessary work at runtime and to make it possible to simplify constructs that are repeated many times.

# Compiling
A shell file is included in src named Make.sh, it will invoke gcc with the provided arguments and all of the sources files needed to compile.
`./Make.sh -oPreprocessor` should be good enough to compile the project into an executable named `Preprocessor`. Any extra arguments can be provided, for example to add optimizations or to modify Lua. Other than the provided shell file, use all of the source files defined in `src` except `lua.c` and `luac.c`.

By default, some platform specific extensions may be used if they are defined. If this needs to be avoided, define the macro `LUA_PREPROCESSOR_AVOID_EXTENSIONS` (even a definition for zero is allowed). For example: `-DLUA_PREPROCESSOR_AVOID_EXTENSIONS`. This has no effect on Lua itself, to make Lua itself avoid using extensions you will need to define the appropriate macros that Lua accepts.

If you wish to bring your own libraries into the preprocessor, this can be done by modifying `Libraries.c` to load your libraries. To change the version of Lua used in the preprocessor, replace the folder in `src` that contains the current version of Lua, and update the includes in the files in `src` to use the new name. Some of the functions used may not exist in older versions of Lua.

# Usage
This information will be generated when invoking the preprocessor with no arguments. `./Preprocessor` will be replaced by the program name.
```
Usage: ./Preprocessor input [output]
Available input options:
  f      use the file 'f' as input, if it doesn't begin with '-'
  -      use the standard input as input
  -- f   use the file 'f' as input
  -b f   use the file 'f' as input, opened in binary mode
  -e in  use the argument 'in' as input
Available output options:
         use the standout output if nothing is provided
  f      use the file 'f' as output, if it doesn't begin with '-'
  -- f   use the file 'f' as output
  -b f   use the file 'f' as output, opened in binary mode
```
The input will be read from the specified file, standard input, or the provided program argument. The output will be written to the specified file, or standard output if nothing is specified. By default, files will be used as text streams. `-b` can be specified before a file to indicate that it should be used as a binary stream. `--` can be specified before a file to allow for files that begin with a hyphen and should be used as text streams. Standard input and standard output will always be used as text streams.

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
The preprocessor fundementally performs token manipulation: a sequence of tokens comes in, and a sequence of tokens is outputted. Here, a token refers to a lexical element of one of the following categories: string literals, names, numeric literals, and symbols. Numeric literals can be subdivided into two further categories: integer literals and floating point literals. The supported syntax and meaning for tokens in the preprocessor is equivalent to Lua with several modifications:
* String literals may contain unescaped end of line sequences, they will be interpreted as if they were escaped.
* The escape sequence `\s` will result in a space character, this is intended to stop a `\z` escape sequnce while adding a space.
* Binary (prefix `0b`/`0B`) and octal (prefix `0o`/`0O`) numeric literals are supported, which work very similarly to hexadecimal numeric literals (even allowing floating point literals in binary and octal).
* Underscore digit separators are allowed in all positions after the first digit of a numeric literal, e.g. `123_456.789_123` or the extreme case `1__2_._3__4_e_+_5_`. Underscores cannot be placed prior to the first digit, preventing numeric literals such as `_123_456` and `._123` that would be ambiguous with names.
* The characters ``@!`?$`` are allowed outside of string literals as single character symbols.
* The character `\` is used to associate a symbol with a 'not now' which influences interpretation by the preprocessor (see below).
* Locales (e.g. set by `os.setlocale`) are not supported. Lua will sometimes (depending upon configuration and character set) use the standard ctype functions which depend upon the locale, for example `isalpha` and `isspace` may be be used to determine whether characters should be allowed in identifiers or spaces, respectively. Lua will sometimes (depending upon configuration) use the standard string to number conversion functions which depend upon the locale such as `strtod`, `strtof`, or `strtold` to convert strings to numbers. In all situations where Lua would parse in a manner that depends upon locale, the preprocessor will parse in the same manner but as if the "C" locale was always in effect. Characters outside of the basic character set (it is assumed the basic character set includes `@`, `` ` ``, and `$` as specified in C23) may still be used in string literals, but when searching for the end of a string literal it will still only consider the bytes that make up the characters which can cause issues for locales where a byte that represents a character inside the basic character set when in the initial shift state could also represent another character or part of another character when in a non-initial shift state (UTF-8 does not have this issue, but for example GB18030 will).
* Floating point literals will have their exact value determined differently than Lua due to using a different algorithm to read the value, and the preprocessor doesn't attempt to retain the original spelling of the floating point literal (for example `$lua(.12==tonumber".12")` may expand to false). Thus, if a floating point literal is within the input tokens and is also part of the output tokens (without being changed) the exact values that Lua interprets with the input spelling and output spelling may be different. If `FLT_RADIX` is a positive integer power of two, floating point literals other than decimal floating point literals will preserve the exact value.

These modifications are only present when the preprocessor is scanning for tokens, the standard Lua compiler will still be the same. When converting tokens into a string representation, numeric and string literals will be written in an unspecified format that is valid in standard Lua. Floating point literals are converted in a manner that best preserves their exact value, at least for systems where `FLT_RADIX` is a positive integer power of two. Names and symbols are written using their exact original spelling. How spaces are inserted between tokens, before the first token, or after the last token is unspecified except that at least one space will be inserted if it is necessary to delimit two tokens.

The preprocessor converts the input file to tokens, scanning left to right and doing macro expansions. The results of a macro expansions are included in this scanning. After all macro expansions, the remaining tokens (the output tokens) are then converted to a string which is written to the output file as a single line (for binary files, a new line character is added at the end). However, if no tokens remain after macro expansion then zero lines are written to the output file instead (for binary files, an empty file).

Each symbol has an associated amount of 'not nows', which indicates that a symbol has no special meaning. These can be directly specified by putting one or more `\` characters before a symbol, each backslash adds one 'not now' to the symbol. Space characters will be ignored, e.g. `\ \ $` is allowed. Same as the other syntax modifications discussed above, these backslashes will only be recognized by the preprocessor and not by the standard Lua compiler. This is intended to suppress macro expansion, or to use unbalanced brackets for macros that use bracketed token sequences. In a context where a symbol always has a special meaning, the number of 'not nows' must be zero. Each time a symbol with 'not nows' is checked for special meaning, a 'not now' will be removed. A symbol shouldn't have 'not nows' when converting it into a string representation, for example when the output tokens are written to the output file.

Macro expansions are done using a `$` symbol with no 'not nows'. After the `$` symbol, one or more string literals or names will be scanned for, delimited by `.` symbols. The first name will be used to index the macros table, the result of which should be a function, built in macro, or table to search further into. If a table is found, then another name will be scanned for and this process repeats but with the previously found table. Scanning will stop when the final string literal or name is found that finds the function or built in macro to call. While scanning, macro expansions will be done, for example: `$$lua("lua")()` is valid.

Once the built in macro or function is found to call, it will be invoked with a reference to the preprocessor state and the number of tables searched through (other than the macros table) to find the macro to invoke. The reference to the preprocessor state allows manipulating the tokens, getting the macros table, setting the macros table to a new table, setting error information, etc. Tokens prior to the `$` symbol are not visible, and cannot be changed nor viewed. A reference to a preprocessor state should only be used when macro expansion with a function is occuring, unless otherwise specified (e.g. you cannot interact with the preprocessor state while a macro is being searched inside of __index except in very limited ways). Functions called by the preprocessor cannot yield the preprocessor, for example by using the function `coroutine.yield`.

When the built in macro or function returns, the visible tokens are the result of macro expansion. Generally, a macro should only try to view and manipulate tokens in a defined region, such as in a set of brackets rather than work with all visible tokens.

Built in macros should only be invoked with their original name or their original name as a string literal, and should only be invoked directly by putting them in the macros table. The default preprocessing state will start with a macros table containing just the built in macros: `now`, `notnow`, `totokens`, `tostring`, `concat`, `if`, `defined`, `lua`, `none`. Here, 'brackets' is used to refer to parentheses, square brackets, or curly brackets. All types of brackets are treated equally, and when counting brackets to find the ending bracket they will be treated equally. Built in macros will scan left to right and do macro expansions, unless stated otherwise. The results of macro expansions are included in this scanning.

# The built in macros
* `none`: Removes the symbol `$` and the string literal or name `none`.
```lua
$none -- Expands to nothing
```
* `lua`: Expects an expression or sequence of statements enclosed with brackets after the string literal or name `lua` which will be evaluated. If both interpretations are valid, the expression interpretation will be preferred. A reference to the preprocessor state is provided in the `...` arguments. References to the preprocessor state can be used, and only the tokens after the closing bracket are visible. The symbol `$`, the string literal or name `lua`, and the expression or sequence of statements enclosed with brackets (including the enclosing brackets themselves) are replaced by the sequence of tokens described by the return value aka the result. The return value should be:

    * nil, the name `nil` is the result.
    * A boolean, if this boolean is false then the name `false` is the result otherwise the name `true` is the result.
    * A number, this number will be used in the result and must not be NaN or an implementation-specific nonfinite value. If the number is a negative floating point number or negative zero the result is four tokens `(-N)` where N is the absolute value of the number as a floating point literal, otherwise it is replaced by a single numeric literal with the same value as the number. If the number is not a negative floating point number or negative zero, the resulting numeric literal is an integer literal if the number is an integer and is a floating point literal if the number is a float.
    * A string, the result is a string literal with the same contents as the string.
    * A table, values are read from the table like it is an array (starting at index 1, incrementing the index until it find the first nil value). Each value should be a string, which will be converted to tokens for the result. The tokens generated from each of the strings are combined to form the result. These strings are evaluated separately, meaning tables such as `{"[[","]]"}` are not valid.
    * Nothing, if zero values are returned then the result is nothing.

    If multiple values are returned, all values except the first are ignored.
```lua
$lua(1+2) -- 3
$lua() -- nothing
$lua(local string = "abc" return string) -- "abc"
$lua(math.abs(-1)) -- 1, this can be evaluated as an expression or a statement so it gets evaluated as an expression
$lua(math.abs(-1);) -- a statement, nothing is returned
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
* `defined`: Expects one or more string literals or names separated by `.` symbols after the string literal or name `defined`, searching just like it was trying to invoke the macro. The symbol `$`, the string literal or name `defined`, and the one or more string literals or names will be replaced by the name `true` if a function or built in macro located correctly was found, or the name `false` otherwise.
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
* `if`: After the symbol `$` this macro is expected to have the following form: an if branch, then zero or more elseif or else branches, and finally a terminating end.

    The terminating end consists entirely of a string literal or name that is `end`. The types of branches have the following forms:
    * An if branch starts with the string literal or name `if` then is followed by a condition and then contents of the branch.
    * An elseif branch starts with a string literal or name that is `elseif` then is followed by a condition and then the contents of the branch.
    * An else branch starts with a string literal or name that is `else` then is followed by the contents of the branch.

    The contents of a branch and conditions both have the form of a bracketed sequence of tokens optionally preceded by a `::` symbol.

    Branches and their components are evaluated left to right. The first if or elseif branch that has a condition of true or else branch is the selected branch. If there are no else branches and all if and elseif branches have conditions of false then there is no selected branch. Each condition which does not occur after the selected branch (when there is a selected branch) shall have either a string literal or name which is either `true` or `false`. Each condition which occurs after the selected branch (when there is a selected branch) has its contents ignored aside from determining when the bracketed sequence of tokens ends, additionally `$` symbols without 'not nows' will not be evaluated within the condition after the first opening bracket if the `::` symbol is not present. The contents of a branch which is not the selected branch are ignored in the same way as a condition that occurs after the selected branch.

    The symbol `$`, the string literal or name `end`, and all tokens in between except the tokens inside of the brackets of the contents of the selected branch (if one was chosen) are removed.
```lua
$if(true){1}else{2}end -- 1
$if(false){1}else{2}end -- 2
$if(false){3}end -- nothing
$if(true){}else{$lua(error())}end -- nothing, no error because the second $ is ignored here
$if(true){}else::{$lua(error())}end -- error, the $ is evaluated anyway because of the ::
$if(false){}elseif(true){1}elseif($lua(error())){}end -- 1, no error because the $ is ignored here (and :: can be used again to force evaluation)
-- branches after an else branch are allowed:
$if(true){}else{}elseif(){}else{}end -- nothing, and the elseif doesn't need anything in the condition when not being checked
$"if"("false"){1}"elseif"("true"){2}"else"{3}"end" -- 2
$if(false){1}$if(true){elseif(false)}else{else}end{2}else{3}end -- 3
```
* `concat`: Expects one or more string literals, or one or more names, in either case terminated by a semicolon and follwing the string literal or name `concat`. The symbol `$`, the symbol `;`, and all tokens in between are replaced by a string or name which is the result of concatenating all of the provided string literals or names. The result is a string literal if all of the inputs are string literals, and a name if all of the inputs are names. Mixing string literals and names in the input is not allowed.
```lua
$concat a b c; -- abc
$concat "a" "b" "c"; -- "abc"
$concat a; -- a
$concat a "b"; -- error, cannot mix string literals and names
```
* `tostring`: Expects a bracketed sequence of tokens after the string literal or name `tostring` which are converted into a string literal. The specific format shouldn't be relied upon, tokens may have multiple ways of being converted into the string, and the way spaces are inserted shouldn't be relied upon. The symbol `$`, the string literal or name `tostring`, and the bracketed sequence of tokens (including the enclosing brackets) are replaced by the string representation of the tokens within the brackets as a string literal.
```lua
$tostring(1+2) -- "0X1+0X2"
$tostring(()) -- "()"
$tostring() -- ""
$tostring($concat a b c;) -- "abc"
$tostring(\$concat a b c;) -- "$concat a b c;"
```
* `totokens`: Expects a string literal after the string literal or name `totokens` which is converted to the tokens it represents, doing the opposite function of `tostring`. The symbol `$`, the string literal or name `totokens`, and the string literal are replaced by the tokens read from the string.
```lua
$totokens"abc" -- abc
$totokens"(1+2)" -- (1+2)
$totokens"$lua(1+2)" -- 3, the $ in the resulting tokens gets expanded after totokens finishes
$totokens$tostring(a b c) -- a b c, tostring and totokens do opposite things
```
* `notnow`: Adds 'not nows' to tokens. The brackets in this description are used to indicate optionality, e.g. `[number]` means a number may or may not appear. In this description `number` refers to a nonnegative integer literal, or a floating point literal that is exactly representable as a nonnegative integer. This macro is overloaded to do multiple things and shall have one of the following forms after the `$` token (`notnow` represents the string literal or name `notnow`):
    * `notnow [number] ;`: Adds the specified number of 'not nows' to the symbol `$`, or one if no number is specified. The string literal or name `notnow`, the number (if specified), and the symbol `;` will be removed.
    * `notnow [number] : symbol`: Adds the specified number of 'not now's to the symbol, or one if no number is specified. The symbol `$`, the string literal or name `notnow`, the number (if specified), and the symbol `:` will be removed.
    * `notnow [number] [::] bracketed token sequence`: Adds the specified number of 'not nows' to the symbols in the bracketed token sequence, or one if no number is specified. If the symbol `::` is not specified, any `$` symbols without 'not nows' will not be evaluated within the bracketed token sequence. The symbol `$`, the string literal or name `notnow`, the number (if specified), the symbol `::` (if specified), and the enclosing brackets will be removed.
    * `notnow [number] ? [::] bracketed token sequence`: Moves the tokens inside of the bracketed token sequence to the end of the visible tokens. Any `$` symbols without 'not nows' will not be evaluated within the bracketed token sequence if the symbol `::` is not specified. After moving the tokens, it will scan them left to right doing macro expansions, then add the specified number of 'not nows' to the symbols, or one if no number is specified. The results of macro expansion are included in this scanning. The tokens after scanning for macro expansions (if any) are placed back, and the symbol `$`, the string literal or name `notnow`, the number (if specified), the symbol `?`, the symbol `::` (if specified), and the enclosing brackets will be removed.
```lua
$notnow;none -- $none, same as \$ here
$tostring($notnow:]) -- "]", same as \] here, this is interesting if the token used is created from another macro
$notnow($lua(1+2)) -- $lua(1+2), just a convient way of adding 'not nows' to some tokens
$notnow::($lua(foo())) -- similar to above, but doing macro expansions so the result depends upon foo
$tostring($notnow?($totokens"(")) -- a simple way of adding 'not nows' to the results of macro expansion
```
* `now`: Expects a bracketed sequence of tokens after the string literal or name `now`. The symbol `$`, the string literal or name `now`, and the enclosing brackets will be removed. This macro is intended to do an extra evaluation to remove 'not nows'.
```lua
$now(\$)none -- nothing
$now(\$lua(1)) -- 1
```

# The preprocessor interface
The default preprocessor state is created by the preprocessor implicitly before converting the input file to tokens, and references to this preprocessor state are provided when invoking macros which are functions or through the built in macro `lua` within the default preprocessor state. The scanning of the tokens obtained from the input file occurs within the default preprocessor state. Methods are provided on each reference to a preprocessor state that will change the preprocessor state or the tokens it contains. Each preprocessor state contains a cursor, which is used to identify the current token being viewed. This cursor will be saved when invoking a macro from within a macro. The cursor can be in the invalid state, indicating that no token is being viewed. When a macro is invoked, the cursor will by default be set to the first visible token, or the invalid state if there are no visible tokens. Every preprocessor state contains a macros table (which must be a table) that is used when searching for macros during macro expansion.

A global function `tokens` is provided that will create a new preprocessor state, and return a reference to it. The macros table is provided with the first argument. By default, this preprocessor state will contain no tokens, and the cursor will be in the invalid state. When no macro is being invoked on this preprocessor state, all tokens are visible and the references to the preprocessor state can be used.
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
    t:handle_dollar() -- invoke this macro using all the tokens that have been set up
    p:copy(t) -- this string will result in 1 token
) 1 -- so it can be copied onto this token after $lua
```

Each preprocessor state contains zero or more tokens, some of these tokens may not be visible at a given point in time. Tokens which are not visible cannot be interacted with in any way by the preprocessor interface. The only way to make tokens invisible is to do macro expansion, and as such it is not possible to make tokens invisible arbitrarily (e.g. making one token in the middle of several others invisible, or making tokens at the end of a sequence of tokens invisible). The purpose of hiding tokens before a macro expansion is so that code which does a macro expansion can assume that everything earlier is unchanged which facilitates left to right parsing and doing macro expansions along the way.

Errors can occur during preprocessing, each preprocessing state can be in an error state. When the preprocessor state enters the error state, a string is stored containing the error information. Information will be added about the currently invoked macros as the error is returning back. The only methods accessible during an error are the methods for getting or setting an error. Errors are not recoverable within a preprocessing state, but can be handled from outside of the preprocessing state. Some possible reasons for an error to occur are: failing to find a built in macro or function to invoke, using a built in macro incorrectly, setting an error with the preprocessor interface, a function erroring during macro expansion, or a memory error.

If a method on a reference to a preprocessor state is used incorrectly or if there is not enough memory (unless otherwise specified) the method will generate an error, but no error will be set on the preprocessor state and the preprocessor state will remain unchanged.

# The methods of the preprocessor interface
All method names use `snake_case`. When a cursor's token is mentioned the cursor must not be in the invalid state, unless stated otherwise.

`get_content()` and `set_content(content)` will get or set (respectively) the content of the cursor's token. For string literals and names, a string with the contents will be used. For integer and floating point literals, integers and floats will be used (respectively). For symbols, their string representations will be used. Floating point literals can never be negative, negative zero, NaN, or any implementation-specific nonfinite value. Setting the content to negative zero will cause the content to be positive zero, instead of an error being generated.

`get_type()` and `set_type(type)` will get or set (respectively) the type of the cursor's token. The types are: `string`, `name`, `integer`, `float`, and `symbol`. `string` corresponds to string literals, `name` corresponds to names, `integer` corresponds to integer literals, `float` corresponds to floating point literals, and `symbol` corresponds to symbols. `set_type` will set the content to a default value: the empty string for string literals, `nil` for names, zero for integer literals, positive or unsigned zero floating point literals, and `$` for symbols.

`get_not_now_amount()` and `set_not_now_amount(nonnegative_integer)` will get or set (respectively) the amount of 'not nows' on the cursor's token. For these methods, a non-symbol is considered to have zero 'not nows' and the amount cannot be changed (only setting to zero is allowed).

`is_valid()` will return `false` if the cursor is in the invalid state, and `true` otherwise.

`make_invalid()` will put the cursor in the invalid state.

`is_advancing_valid()` and `is_retreating_valid()` will return `false` if advancing or retreating (respectively) will leave the the cursor in the invalid state, and `true` otherwise. The cursor must not be in the invalid state.

`go_to_start()` and `go_to_end()` will go the the start or end (respectively) of the visible tokens, or end up in the invalid state if there are no visible tokens.

`advance()` and `retreat()` will advance or retreat (respectively), or end up in the invalid state if there are no visible tokens after or before (respectively) the cursor's token.

`remove_and_advance()` and `remove_and_retreat()` will remove the cursor's token and advance or retreat (respectively), or end up in the invalid state if there are no visible tokens after or before (respectively) the cursor's token.

`insert_at_start()` and `insert_at_end()` will insert a new token at the start or end (respectively) of the visible tokens, and set the cursor to point at the new token. The token will start as an integer literal with the value zero. If there are no visible tokens before the call then the inserted token will be the only visible token. The inserted token will be placed after all invisible tokens.

`insert_ahead()` and `insert_behind()` will insert a new token ahead or behind (respectively) of the cursor's token, and set the cursor to point at the new token. The token will start as an integer literal with the value zero.

`insert_at_start_and_stay()`, `insert_at_end_and_stay()`, `insert_ahead_and_stay()`, and `insert_behind_and_stay()` are the same as the methods without the `_and_stay` suffix, but the cursor will not change.

`steal_to_start_and_advance(other_preprocessing_state)`, `steal_to_end_and_advance(other_preprocessing_state)`, `steal_ahead_and_advance(other_preprocessing_state)`, `steal_behind_and_advance(other_preprocessing_state)`, `steal_to_start_and_retreat(other_preprocessing_state)`, `steal_to_end_and_retreat(other_preprocessing_state)`, `steal_ahead_and_retreat(other_preprocessing_state)`, and `steal_behind_and_retreat(other_preprocessing_state)`: The cursor's token is taken from the other preprocessing state, and moved into the provided preprocessing state (the self argument). The token is moved to the location specified by the first part of the name same as the `insert_` methods (for `ahead` and `advance` the cursor must not be in the invalid state), and the cursor is set to point at this token. The cursor of the other preprocessing state will be advanced or retreated according to the last part of the name similar to `advance()` and `retreat()`. The other preprocessing state must not be the same as the provided preprocessing state, must be in a state where the preprocessing interface can be used with it, and its cursor shall not be in the invalid state.

`handle_dollar()` will do a macro expansion. The cursor's token must be a `$` symbol without any 'not nows'. The cursor will point at the first token of the macro expansion, or will be set in the invalid state if the macro expansion is empty. If an error is set during macro expansion, this method will raise an error.

`handle_dollar_and_not_nows()` will do macro expansion like `handle_dollar()` zero or more times until the cursor's token is in the invalid state or is not a `$` symbol without any 'not nows'. Then, if the cursor's token is not in the invalid state and is a symbol with one or more 'not nows', one 'not now' will be removed and `true` will be returned. In all other cases, `false` will be returned.

`copy(other_preprocessing_state)` will copy the type, contents, and 'not nows' (for symbols) of the provided preprocessing state's cursor's token into the cursor's token (of the preprocessing state which the method was used on). The other preprocessing state must be in a state where the preprocessing interface can be used with it.

`shift_to_start()`, `shift_to_start_and_advance()`, `shift_to_start_and_retreat()`, `shift_to_end()`, `shift_to_end_and_advance()`, `shift_to_end_and_retreat()`: Shifts the cursor's token to the start or end of the visible tokens as it says in the name. The cursor will follow the token as it shifts for `shift_to_start()` and `shift_to_end()`, otherwise it will advance or retreat as it says in the name similar to `advance()` and `retreat()` (the new cursor is decided before shifting, e.g. calling `shift_to_end_and_advance` while the cursor's token has nothing after it results in the cursor being in the invalid state and no tokens actually being shifted).

`swap_with_start()`, `swap_with_end()`, `swap_ahead()`, `swap_behind()`: Swaps the type, content, and 'not nows' (for symbols) of the cursor's token with the token specified in the name. For `swap_ahead()` and `swap_behind()`, there must be tokens ahead of and behind (respectively) the cursor's token. For `swap_with_start()` and `swap_with_end()`, if the cursor's token is the starting or ending token (respectively) then nothing will happen.

`swap_between(other_preprocessing_state)` swaps the type, content, and 'not nows' (for symbols) of the cursor's token and the cursor's token of the other preprocessing state. The other preprocessing state must be in a state where the preprocessing interface can be used with it.

`get_macros()` and `set_macros(table)` will get or set (respectively) the macros table.

`get_error()` will get the current error message contained in the preprocessing state, or `nil` if no error exists. This method can always be used, even when the rest of the preprocessor interface cannot.

`set_error(string)` will set the error message contained in the preprocessing state. This method will not raise an error in the calling Lua code assuming no memory errors occur (and the reference to the preprocessor state can be used), if this type of quick exiting is desired then an error should be created explicitly. If a memory error does occur then the error message will be set to a string that indicates a memory error occured, an error will be raised, and all future calls to `set_error` on the same preprocessing state will also raise a memory error (because it would be impossible to recover the error information lost before).

`clear()` will remove every visible token, and set the cursor in the invalid state.
