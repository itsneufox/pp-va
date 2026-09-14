#include <pp-va>

stock LogMessage(const template[], AnyTag:...)
{
    new output[144];
    PP_Format(output, sizeof(output), template, 1);
    print(output);
    return 1;
}

main() {}

public OnGameModeInit()
{
    LogMessage("%s earned $%d at %.2f%%", "Alice", 500, 12.5);
    return 1;
}
