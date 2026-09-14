#if defined PP_VA_INCLUDED
    #endinput
#endif
#define PP_VA_INCLUDED

#include <open.mp>
#include <PawnPlus>

stock List:PP_GetArgumentReferences(first, levels = 1)
{
    new List:arguments = pawn_get_args("+", false, levels);
    new count = list_size(arguments);
    list_delete(arguments);
    if (count <= first) return list_new();

    new String:signature = str_new("");
    str_resize(signature, count, 'i');
    arguments = List:pawn_call_native("pawn_get_args", "Sii", signature, false, levels);
    list_remove_range(arguments, 0, first);
    return arguments;
}

// Preserve open.mp format semantics for existing array-based call sites.
stock PP_Format(output[], size, const template[], first, levels = 1)
{
    new List:arguments = PP_GetArgumentReferences(first, levels + 1);
    if (!list_size(arguments))
    {
        list_delete(arguments);
        return format(output, size, "%s", template);
    }
    new result = pawn_call_native("format", "aisl", output, size, template, arguments);
    list_delete(arguments);
    return result;
}

stock PP_Printf(const template[], first, levels = 1)
{
    new List:arguments = PP_GetArgumentReferences(first, levels + 1);
    if (!list_size(arguments))
    {
        list_delete(arguments);
        return printf("%s", template);
    }
    new result = pawn_call_native("printf", "sl", template, arguments);
    list_delete(arguments);
    return result;
}

#if !defined PP_VA_RETURN_SIZE
    #define PP_VA_RETURN_SIZE (144)
#endif
#if PP_VA_RETURN_SIZE < 1
    #error PP_VA_RETURN_SIZE must include room for the string terminator.
#endif

stock PP_FormatReturn(const template[], first, levels = 1)
{
    new output[PP_VA_RETURN_SIZE];
    PP_Format(output, sizeof(output), template, first, levels + 1);
    return output;
}
