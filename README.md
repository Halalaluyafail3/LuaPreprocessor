# LuaPreprocessor
A preprocessor for Lua

# Compiling
A shell file is included in src named Make.sh, it will invoke gcc with the provided arguments and all of the sources files needed to compile:
`./Make.sh -o Preprocessor` should be good enough to compile the project into an executable named `Preprocessor`. Extra arguments can be added to add macros that will adjust Lua to appropriate settings, or the config file can be editted.
