# pp-va

Variadic argument forwarding for Pawn using PawnPlus. Provides reusable formatting
helpers without assembly, hook libraries, or a localization dependency.

## Requirements and installation

- open.mp and its standard includes.
- [PawnPlus](https://github.com/IS4Code/PawnPlus): install a matching include/plugin
  pair and load the plugin. Tested with 1.5.3.
- Pawn compiler 3.10.11 is the tested compiler.

Copy `includes/pp-va.inc` into your compiler's include path, then use
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

For other native calls, use `PP_GetArgumentReferences()` and PawnPlus's list
argument expansion. Arbitrary-call stack splicing and mixed explicit/forwarded
argument lists are not provided.

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

## Tests

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

## License

[MIT](LICENSE) © 2026 itsneufox.

## AI disclosure

AI tools assisted with parts of the code and documentation. Review the source and test the library on your own server before using it in production.
