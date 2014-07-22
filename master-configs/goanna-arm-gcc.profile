{
  "NAME": "Linux GCC",

  "ALIASES": [
    "cdt.managedbuild.toolchain.gnu.base",
    "Cross GCC",
    "GNU Autotools Toolchain",
    "MacOSX GCC",
    "Solaris GCC",
    "No ToolChain"
  ],

  "GOANNA": {
    "ARGUMENTS": [ "--timeout=240", "--internal-error=0", "--parse-error=0" ]
  },

  "COMPILER": {
    "c": {
      "EXECUTABLE": {
        "command": "which arm-linux-gnueabi-gcc",
        "read": {
          "unit": "string",
          "from": "stdout"
        },
        "replace": {
          "string": "string",
          "regex" : "\r?\n",
          "with"  : ""
        }
      },
      "ARGUMENTS": "",
      "VERSION": {
        "FILE": "__GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__",
        "command": "\"%COMPILER.c.EXECUTABLE%\" -E -P %COMPILER.c.ARGUMENTS% \"%FILE%\"",
        "read": {
          "unit": "string",
          "from": "stdout"
        },
        "replace": {
          "string": "string",
          "regex" : "[\r\n\t ]+",
          "with"  : ""
        }
      }
    },
    "c++": {
      "EXECUTABLE": {
        "command": "which arm-linux-gnueabi-g++",
        "read": {
          "unit": "string",
          "from": "stdout"
        },
        "replace": {
          "string": "string",
          "regex" : "\r?\n",
          "with"  : ""
        }
      },
      "ARGUMENTS": "",
      "VERSION": {
        "FILE": "__GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__",
        "command": "\"%COMPILER.c++.EXECUTABLE%\" -E -P %COMPILER.c++.ARGUMENTS% \"%FILE%\"",
        "read": {
          "unit": "string",
          "from": "stdout"
        },
        "replace": {
          "string": "string",
          "regex" : "[\r\n\t ]+",
          "with"  : ""
        }
      }
    }
  },

  "PARSER": {
    "c": {
      "EXECUTABLE": "edg_linux",
      "ARGUMENTS": [
        "-D__builtin_expect(exp,c)=(exp)",
        "--gcc",
        {
          "command": "expand \"--gnu_version=%COMPILER.c.VERSION%\"",
          "read": {
            "unit": "string",
            "from": "stdout"
          },
          "replace": {
            "string": "string",
            "regex" : "[\r\n\t ]+",
            "with"  : ""
          },
          "replace": {
            "string": "string",
            "regex" : "\\.",
            "with"  : "0"
          }
        }
      ],
      "sys_include_prefix": "--sys_include",
      "INCLUDES": {
        "FILE": "#include <stdio.h>",
        "command": "\"%COMPILER.c.EXECUTABLE%\" -v -c %COMPILER.c.ARGUMENTS% \"%FILE%\"",

        "read": {
          "unit": "string",
          "from": "stderr"
        },

        "split": {
          "string": "string list",
          "regex": "\r?\n"
        },

        // restrict output to between these lines
        "between": {
          "string list": "string list",
          "start": "#include <...>",
          "end"  : "^[^ ]"
        },

        // strip white space
        "map": {
          "string list": "string list",
          "regex": "^[ \t]*\\(.*\\)[ \t]*$"
        }
      },

      // do not pass the parser --c++0x option if in C mode
      "ARGUMENTS_FILTER_OUT": "^--c\\+\\+0x$",

      // predefined macros
      "MACROS": {
        "FILE": "",
        "command": "\"%COMPILER.c.EXECUTABLE%\" %COMPILER.c.ARGUMENTS% \"%FILE%\" -E -dM",

        "read": {
          "unit": "string",
          "from": "stdout"
        },

        "split": {
          "string": "string list",
          "regex": "\r?\n"
        },

        "sort": {
          "string list": "string list"
        },

        // filter out definitions that EDG can't handle
        "filter out": {
          "string list": "string list",
          "regex": "^#define \\(__cplusplus\\|__GNUC_MINOR__\\|__GNUC__\\|__GNUG__\\|__VERSION__\\|__STDC_HOSTED__\\|__STDC__\\|__GNUC_PATCHLEVEL__\\|__WCHAR_TYPE__\\|__GXX_RTTI\\|__CHAR16_TYPE__\\|__CHAR32_TYPE__\\|__GNUC_GNU_INLINE__\\|__EXCEPTIONS\\)"
        },

        // map to key-value pairs
        "map": {
          "string list": "string object",
          "regex": "^#define \\([^ ]*\\) \\(.*\\)$"
        }
      }
    },
    "c++": {
      "EXECUTABLE": "edg_linux",
      "ARGUMENTS": [
        "-D__builtin_expect(exp,c)=(exp)",
        "--g++",
        "--no_parse_templates",
        {
          "command": "expand \"--gnu_version=%COMPILER.c++.VERSION%\"",
          "read": {
            "unit": "string",
            "from": "stdout"
          },
          "replace": {
            "string": "string",
            "regex" : "[\r\n\t ]+",
            "with"  : ""
          },
          "replace": {
            "string": "string",
            "regex" : "\\.",
            "with"  : "0"
          }
        }
      ],
      "sys_include_prefix": "--sys_include",
      "INCLUDES": {
        "FILE": "#include <iostream>",
        "command": "\"%COMPILER.c++.EXECUTABLE%\" -c %COMPILER.c++.ARGUMENTS% \"%FILE%\" -v",

        "read": {
          "unit": "string",
          "from": "stderr"
        },

        "split": {
          "string": "string list",
          "regex": "\r?\n"
        },

        // restrict output to between these lines
        "between": {
          "string list": "string list",
          "start": "#include <...>",
          "end"  : "^[^ ]"
        },

        // strip white space
        "map": {
          "string list": "string list",
          "regex": "^[ \t]*\\(.*\\)[ \t]*$"
        }
      },
      // do not pass the parser --c89/--c99 option if in C++ mode
      "ARGUMENTS_FILTER_OUT": "^--c[89]9$",
      "MACROS": {
        "FILE": "",
        "command": "\"%COMPILER.c++.EXECUTABLE%\" %COMPILER.c++.ARGUMENTS% \"%FILE%\" -E -dM",

        "read": {
          "unit": "string",
          "from": "stdout"
        },

        "split": {
          "string": "string list",
          "regex": "\r?\n"
        },

        "sort": {
          "string list": "string list"
        },

        // filter out definitions that EDG can't handle
        "filter out": {
          "string list": "string list",
          "regex": "^#define \\(__cplusplus\\|__GNUC_MINOR__\\|__GNUC__\\|__GNUG__\\|__VERSION__\\|__STDC_HOSTED__\\|__STDC__\\|__GNUC_PATCHLEVEL__\\|__WCHAR_TYPE__\\|__GXX_RTTI\\|__CHAR16_TYPE__\\|__CHAR32_TYPE__\\|__GNUC_GNU_INLINE__\\|__EXCEPTIONS\\)"
        },

        // map to key-value pairs
        "map": {
          "string list": "string object",
          "regex": "^#define \\([^ ]*\\) \\(.*\\)$"
        }
      }
    }
  },

  "INTERNAL_PARSER_ARGUMENT_TRANSLATION_SPECIFICATION": {
    // flags that take no argument, shared with the Parser
    "NO_ARG_PARSER": [ "-C", "-P", "-H", "-w" ],
    // one-letter flags that take an argument, shared with the Parser
    "ONECHAR_ARG_PARSER": [ "D", "U", "I" ],
    // one-letter flags that take an argument, not shared with the Parser
    "ONECHAR_ARG_NO_PARSER": [ "A", "l", "u", "b", "V", "G", "o" ],
    // flags that take no argument, not shared with EDG
    "NO_ARG_NO_PARSER": [
      "-MD", "-MP", "-MMD", "-MG", "-specs=.+", "-I-",
      "--sysroot=.+", "-m32", "-m64"
    ],
    // multi-letter flags that take an argument, not shared with the Parser
    "ARG_NO_PARSER": [
      "-MF", "-MQ", "-MT", "-aux-info", "-arch", "-Xpreprocessor",
      "-idirafter", "-iprefix", "-withprefix", "-iwithprefixbefore",
      "-Xassembler", "-Xlinker", "-rdynamic", "--param"
    ],
    // do not run analysis
    "NO_ANALYZE": [ "-E", "-M", "-MM" ],
    // flags with argument translated for the Parser
    "TRANSLATE_ARG": {
      "-imacros": "--preinclude_macros",
      "-include": "--preinclude",
      "-iquote" : "-I",
      "-isystem": "--sys_include"
    },
    // flags: no argument, translated for the Parser
    "TRANSLATE_NO_ARG": {
      "-ansi": "--c89",
      "-std=c89": "--c89",
      "-std=iso9899:1990": "--c89",
      "-std=gnu89": "--c89",
      "-std=c99": "--c99",
      "-std=c9x": "--c99",
      "-std=iso9899:1999": "--c99",
      "-std=iso9899:199x": "--c99",
      "-std=gnu99": "--c99",
      "-std=gnu9x": "--c99",
      "-std=c++0x": "--c++0x",
      "-std=gnu++0x": "--c++0x",
      "-std=c++11": "--c++0x",
      "-std=gnu++11": "--c++0x",
      "-ffor-scope": "--new_for_init",
      "-fno-for-scope": "--old_for_init"
    },
    // change language for subsequent file. e.g. "-x c test.cc" will treat test.cc as a c file.
    "LANGUAGE": "-x",
    // pass all other one-dash args to compiler
    "ONE_DASH_ARGS": true
  }
}
