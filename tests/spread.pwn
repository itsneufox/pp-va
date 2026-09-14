#include <pp-va>
#pragma dynamic 16384
new failures;
stock Check(bool:value, const label[])
{
    if(!value) { failures++; printf("FAIL: %s", label); }
}
stock Forward(output[], const template[], AnyTag:...)
{
    format(output, 128, template, ___(2));
}
stock Mixed(output[], AnyTag:...)
{
    format(output, 128, "%d:%s:%d:%.2f:%d", 5, ___(1), 8);
}
stock Inner(AnyTag:...)
{
    return getarg(0) + getarg(1);
}
stock Sum(AnyTag:...)
{
    new value;
    for(new i; i<numargs();i++) value+=getarg(i);
    return value;
}
stock Nested(AnyTag:...)
{
    return Sum(Inner(___(0)), ___(0), 9);
}
stock Recurse(depth, AnyTag:...)
{
    if(depth) return Recurse(depth-1, ___(1));
    return Sum(___(1));
}
stock ArrayReturn(const template[], AnyTag:...)
{
    new output[128];
    format(output, sizeof(output), template, ___(1));
    return output;
}
stock PassArray(const template[], AnyTag:...)
{
    return ArrayReturn(template, ___(1));
}
stock Change(AnyTag:...)
{
    setarg(0,0,42);
    return getarg(1, 2);
}
stock Reference(&value, const text[])
{
    #pragma unused value, text
    return Change(___(0));
}
stock PrintForward(const template[], AnyTag:...)
{
    return printf(template, ___(1));
}
stock String:DynamicForward(const template[], AnyTag:...)
{
    return str_format(template, ___(1));
}
forward InvalidSkip();
public InvalidSkip()
{
    return Sum(___(1));
}
forward BadTarget(AnyTag:...);
public BadTarget(AnyTag:...)
{
    #emit HALT 10
    return 0;
}
forward TargetError();
public TargetError()
{
    return BadTarget(___(0));
}

main() {}
public OnGameModeInit()
{
    new output[128];
    Forward(output, "%s:%d:%.2f", "hello", 42, 1.25);
    Check(!strcmp(output,"hello:42:1.25"),"direct format");
    Mixed(output, "hello", 42, 1.25);
    Check(!strcmp(output,"5:hello:42:1.25:8"),"mixed explicit and spread");
    Check(Nested(2,3)==19,"nested calls");
    Check(Recurse(6,2,3)==5,"recursive forwarding");
    Check(!strcmp(ArrayReturn("%s:%d","array",7),"array:7"),"array return");
    Check(!strcmp(PassArray("%s:%d","array",8),"array:8"),"forward array-returning call");
    Forward(output,"literal");
    Check(!strcmp(output,"literal"),"empty spread");
    new value;
    Check(Reference(value,"abc")=='c' && value==42,"reference mutation");
    for(new i; i<10000;i++) Check(Nested(2,3)==19,"repeated stack cleanup");
    PrintForward("PP_VA_SPREAD_PRINTF %s %d", "works", 42);
    new String:dynamic = DynamicForward("{0:S}:{1:d}", str_new("dynamic"), 9);
    str_get(dynamic, output);
    Check(!strcmp(output, "dynamic:9"), "PawnPlus dynamic string target");
    new ignored;
    Check(pawn_try_call_public("InvalidSkip", ignored, "") == amx_err_native, "invalid skip is rejected");
    for(new i; i<256; i++)
    {
        Check(pawn_try_call_public("TargetError", ignored, "") == amx_err_native, "target error is caught");
        Check(Nested(2,3)==19, "spread resumes after caught target error");
    }
    printf("PP_VA_SPREAD_RESULT failures=%d",failures);
    SendRconCommand("exit");
    return 1;
}
