#pragma dynamic 16384
#include <pp-va>

new failures;

stock Check(bool:condition, const description[])
{
    if (!condition)
    {
        failures++;
        printf("FAIL: %s", description);
    }
}

stock CheckFormat(const expected[], const template[], OPEN_MP_TAGS:...)
{
    new output[256];
    PP_Format(output, sizeof(output), template, 2);
    Check(!strcmp(output, expected), "variadic formatted output");
}

stock ArrayFormat(const template[], OPEN_MP_TAGS:...)
{
    new output[256];
    PP_Format(output, sizeof(output), template, 1);
    return output;
}

stock ForwardFormat(const template[], first)
{
    new output[256];
    PP_Format(output, sizeof(output), template, first, 2);
    return output;
}

stock CheckForward(const expected[], const template[], OPEN_MP_TAGS:...)
{
    Check(!strcmp(ForwardFormat(template, 2), expected), "two-level variadic forwarding");
}

stock CheckBounded(const template[], OPEN_MP_TAGS:...)
{
    new output[5];
    PP_Format(output, sizeof(output), template, 1);
    Check(!strcmp(output, "abcd"), "bounded output is truncated and terminated");
}

stock String:DynamicFormat(const template[], AnyTag:...)
{
    new List:arguments = PP_GetArgumentReferences(1);
    new result;
    new amx_err:error = pawn_try_call_native("str_format", result, "sl", template, arguments);
    list_delete(arguments);
    Check(error == amx_err_none, "raw references forwarded to PawnPlus formatter");
    return String:result;
}

stock CheckEmptyReferences()
{
    new List:arguments = PP_GetArgumentReferences(0);
    Check(list_size(arguments) == 0, "empty caller arguments return an empty list");
    list_delete(arguments);
}

stock CheckWrapperFormat(const expected[], const template[], AnyTag:...)
{
    new output[128];
    PP_Format(output, sizeof(output), template, 2);
    Check(!strcmp(output, expected), "PP_Format forwards wrapper arguments");
}

stock ReturnMessage(const template[], AnyTag:...)
{
    return PP_FormatReturn(template, 1);
}

stock PrintMessage(const template[], AnyTag:...)
{
    return PP_Printf(template, 1);
}

stock ReturnForwardInner(const template[], first)
{
    return PP_FormatReturn(template, first, 2);
}

stock ReturnForwardOuter(const template[], AnyTag:...)
{
    return ReturnForwardInner(template, 1);
}

main() {}

public OnGameModeInit()
{
    CheckWrapperFormat("Alice has 42 at 1.25%", "%s has %d at %.2f%%", "Alice", 42, 1.25);
    CheckWrapperFormat("%d remains", "%d remains");
    Check(!strcmp(ReturnMessage("%s:%d", "returned", 7), "returned:7"), "PP_FormatReturn forwards through an array-returning caller");
    Check(!strcmp(ReturnMessage("%d remains"), "%d remains"), "PP_FormatReturn with no optional arguments preserves template");
    Check(!strcmp(ReturnForwardOuter("%s:%d", "nested", 9), "nested:9"), "PP_FormatReturn caller-depth forwarding");
    PrintMessage("PP_VA_PRINTF_CHECK %s %d %.2f%%", "Alice", 42, 1.25);
    PrintMessage("PP_VA_PRINTF_LITERAL %d %%");
    CheckFormat("hello", "hello");
    CheckFormat("%s and %% remain literal", "%s and %% remain literal");
    CheckFormat("name=Alice n=42 f=1.25 %", "name=%s n=%d f=%.2f %%", "Alice", 42, 1.25);
    CheckFormat("[] -7 0005", "[%s] %d %04d", "", -7, 5);
    Check(!strcmp(ArrayFormat("%s:%d", "array", 7), "array:7"), "array-returning variadic function");
    CheckForward("Alice has 42", "%s has %d", "Alice", 42);
    CheckBounded("%s", "abcdefgh");
    CheckEmptyReferences();
    new String:name = str_new("Alice");
    new String:message = DynamicFormat("{1:d} for {0:S}", name, 42);
    new output[128];
    str_get(message, output);
    Check(!strcmp(output, "42 for Alice"), "PawnPlus dynamic strings and positional arguments");
    SetTimer("FinishTests", 1, false);
    return 1;
}

forward FinishTests();
public FinishTests()
{
    CheckFormat("next callback: 9", "%s: %d", "next callback", 9);
    printf("PP_VA_TEST_RESULT failures=%d", failures);
    SendRconCommand("exit");
    return 1;
}
