# pp-va

Variadic argument forwarding for Pawn using PawnPlus. Provides reusable formatting
helpers backed by PawnPlus, plus optional direct argument spreading.

## Requirements and installation

- open.mp and its standard includes.
- [PawnPlus](https://github.com/IS4Code/PawnPlus): install a matching include/plugin
  pair and load the plugin. Tested with 1.5.3.
- Pawn compiler 3.10.11 is the tested compiler.

Copy the contents of `includes/` into your compiler's include path, including
the bundled `amx_assembly/` directory, then use
`#include <pp-va>`. This is an include library, not a separate server plugin.

## Formatting a wrapper's arguments

```pawn
#include <pp-va>

stock SendFormatted(playerid, colour, const template[], OPEN_MP_TAGS:...)
{
    new output[144];
    PP_Format(output, sizeof(output), template, 3);
    return SendClientMessage(playerid, colour, "%s", output);
}

// The first three parameters are fixed; the remaining ones are forwarded.
// SendFormatted(playerid, -1, "%s earned $%d", "Alice", 500);
```

## Printing and returning formatted arrays

```pawn
stock SendFormatted(playerid, colour, const template[], AnyTag:...)
{
    new output[144];
    PP_Format(output, sizeof(output), template, 3);
    return SendClientMessage(playerid, colour, "%s", output);
}

stock MakeMessage(const template[], AnyTag:...)
{
    return PP_FormatReturn(template, 1);
}

stock LogMessage(const template[], AnyTag:...)
{
    return PP_Printf(template, 1);
}
```

`PP_Printf(template, first, levels = 1)` and
`PP_FormatReturn(template, first, levels = 1)` use the same caller-depth convention
as `PP_Format`. `PP_FormatReturn` returns an array with `PP_VA_RETURN_SIZE` cells
(default 144, including the terminator). Define that constant before including
pp-va to choose another size. All three wrappers preserve the template literally
when no optional arguments are present.

For other native calls without direct spreading, use `PP_GetArgumentReferences()`
and PawnPlus's list argument expansion.

## Direct argument spreading

Direct argument spreading is enabled by default:

```pawn
#include <pp-va>

stock LogMessage(const template[], AnyTag:...)
{
    return printf(template, ___(1));
}
```

`___(n)` forwards the caller's arguments starting at index `n`. In this example,
`1` skips `template`. `___` without parentheses starts at index zero. The skip
count must be a compile-time constant. The forwarded cells must be references,
as they are in `AnyTag:...`, arrays, and `&` parameters; skip fixed scalar values.

Arguments can appear before and after a spread:

```pawn
stock PrintBetween(AnyTag:...)
{
    printf("%d %s %d %d", 5, ___(0), 8);
}
// PrintBetween("hello", 42); prints: 5 hello 42 8
```

The target must accept the expression in a variadic parameter position. This does
not bypass Pawn's type checking or fill arbitrary fixed parameter declarations.
Use one spread per target call. Nested calls containing their own spreads,
recursive forwarding, and array-returning functions are supported.

The required headers from [pawn-lang/amx_assembly](https://github.com/pawn-lang/amx_assembly)
are bundled in `includes/amx_assembly/`, with their original copyright and license
notices. No separate installation is needed. If you define `PP_VA_DISABLE_SPREAD`,
only `pp-va.inc` is needed from this package. Spreading scans and rewrites call
sites when PawnPlus initializes the script, which adds startup work.

Tested with the 32-bit Pawn 3.10.11 interpreter on Linux/open.mp and compiler
optimization levels `-O0` and `-O1`. `-O2`, JIT execution, and suspending a PawnPlus
task during a spread call are not supported. Unsupported marker calls abort the
current invocation instead of forwarding an ordinary integer by mistake.

`PP_VA_MAX_NESTED_SPREADS` defaults to 4 nested spread expressions;
`PP_VA_MAX_SPREAD_FRAMES` defaults to 128 active spread frames. Define these before
including pp-va if needed. The standalone AMX scanner requires
`CODE_SCAN_MAX_PATTERN >= 64`; pp-va sets it to 64 when it is not already defined.
Allow at least 8192 cells of stack/heap for initialization with the default limits.

To disable spreading, define `PP_VA_DISABLE_SPREAD` before including pp-va:

```pawn
#define PP_VA_DISABLE_SPREAD
#include <pp-va>
```

The `PP_*` helpers still work without the AMX dependency. When moving to a compiler
with its own spreading support, use this switch and adapt calls to the compiler's
syntax as needed. pp-va makes no assumption about future compiler syntax.

## API

### PP_Format(output[], size, const template[], first, levels = 1)

Formats the target caller's optional arguments with open.mp's `format` native.
`first` is the number of fixed parameters in that caller, not this helper.
`levels = 1` selects the direct caller; use 2 to forward from its caller.
The return value is the underlying `format` result.

Strings, integers, floats, widths, precision and escaping follow open.mp formatting
rules. The output is bounded by `size`, including its terminator. When there are no
optional arguments, the template is copied literally: `%d` and `%%` remain intact.

### List:PP_GetArgumentReferences(first, levels = 1)

Returns a PawnPlus list of raw argument cells after the fixed parameters. Pawn's
variadic parameters are references, so these cells hold addresses, not copied
string or scalar values. This allows a target native to interpret its own formats.

```pawn
stock String:DynamicFormat(const template[], AnyTag:...)
{
    new List:arguments = PP_GetArgumentReferences(1);
    new result;
    new amx_err:error = pawn_try_call_native("str_format", result, "sl", template, arguments);
    list_delete(arguments);
    if (error != amx_err_none) return str_new(template);
    return String:result;
}
```

The caller owns the returned list, including an empty list. Call `list_delete`
when finished. Deleting the list does not delete the original arguments.
Consume it synchronously while the selected caller and its argument storage remain
alive. Never save it for a timer, retain it after that caller returns, or suspend a
PawnPlus task while using it. Do not dereference these cells as owned values.
`first` must be nonnegative and `levels` must select an existing caller frame.

The small temporary signature string is cleaned up by PawnPlus automatically.
The lists owned by `PP_Format` are released internally on its normal return path.

For deeper forwarding, the depth is relative to the helper being called:

```pawn
stock FormatOuter(output[], size, const template[], first)
{
    return PP_Format(output, size, template, first, 2);
}

stock Outer(const template[], OPEN_MP_TAGS:...)
{
    new output[144];
    FormatOuter(output, sizeof(output), template, 1);
    print(output);
}
```

## Releases

Push a version tag on a commit containing the release workflow to publish a GitHub release:

```bash
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

Use `vMAJOR.MINOR.PATCH` for stable releases, or append `-alpha.N`, `-beta.N`
or `-rc.N` for prereleases (for example, `v1.1.0-beta.1`).
The workflow publishes ZIP and tar.gz archives containing the include, example,
documentation, license and dependency list, plus `SHA256SUMS` and automatically
generated release notes. The required `amx_assembly` headers are included;
open.mp and PawnPlus must be installed separately.

## Tests

Use `--include-dir /path/to/includes` when dependencies are installed outside
the compiler's default include directory. This option can be repeated.

```bash
python3 tests/run.py --server-root /path/to/openmp-server
```

The Linux server directory must contain `omp-server`, `qawno/pawncc`, standard
components directly in `components/`, and PawnPlus and CrashDetect in `plugins/`.
For a custom component layout, pass `--components-dir /path/to/components`.
The runner compiles a fixture and starts a temporary server on an ephemeral
localhost port. It never launches your real gamemode or connects to a database.
Tests cover printf output, scalar/string forwarding, nested callers, array-returning functions,
no-argument templates, empty strings, bounded output and PawnPlus dynamic formats.

To test spreading with the bundled AMX headers, add `--spread`. Run with
`--optimization 0` and `--optimization 1` to check both supported modes. The normal
suite verifies that `___` is not defined when `PP_VA_DISABLE_SPREAD` is defined.

## License

The original code is available under [MIT](LICENSE) © 2026 itsneufox.
The `pp-va.inc` contains code adapted from YSI's y_va by Alex "Y_Less" Cole and contributors and is distributed under [MPL 1.1](LICENSE). Attribution is preserved in the source header. The bundled AMX headers retain their original license notices.

## AI disclosure

AI tools assisted with parts of the code and documentation. Review the source and test the library on your own server before using it in production.
