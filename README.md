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
Input is specified by a file name, stdin with `-`, or as a program argument with `-e`. Output is a file name, or stdout if nothing is provided. In the exceptional case where a file name begins with `-`, a special case is made for `--` to interpret the next argument as a file name.

Example: `./Preprocessor foo.lua out.lua` reads from `foo.lua` and writes to `out.lua`

# What the preprocessor does
TODO

# Examples
```lua
local hpi = $lua(math.pi/2) -- sets hpi to half of pi, this division is guaranteed to happen at compile time
```
