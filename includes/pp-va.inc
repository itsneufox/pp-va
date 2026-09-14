// pp-va: variadic forwarding and optional direct argument spreading.
// This include contains MPL-1.1 code adapted from YSI y_va.

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

// Define PP_VA_DISABLE_SPREAD before including pp-va to use only the PP_* helpers.
#if !defined PP_VA_DISABLE_SPREAD

#if defined PP_VA_SPREAD_INCLUDED
    #endinput
#endif
#define PP_VA_SPREAD_INCLUDED

#if defined ___
    #error Another implementation already defines ___. Define PP_VA_DISABLE_SPREAD.
#endif
#if !defined CODE_SCAN_MAX_PATTERN
    #define CODE_SCAN_MAX_PATTERN (64)
#endif
#if CODE_SCAN_MAX_PATTERN < 64
    #error pp-va spread requires CODE_SCAN_MAX_PATTERN >= 64 before including amx/codescan.
#endif
#if !defined CODE_SCAN_MAX_PARALLEL
    #define CODE_SCAN_MAX_PARALLEL (16)
#endif
#tryinclude <amx_assembly\codescan>
#if !defined CODESCAN_INC
    #include <amx\codescan>
#endif

#define ___ PP_VA_DummyPush()
#define PP_VA_DummyPush()(%0) PP_VA_DummyPush((%0)*cellbytes)

#if !defined PP_VA_MAX_NESTED_SPREADS
    #define PP_VA_MAX_NESTED_SPREADS (4)
#endif
#if PP_VA_MAX_NESTED_SPREADS < 1
    #error PP_VA_MAX_NESTED_SPREADS must be positive.
#endif

static stock
	PP_VA_g_sLength[PP_VA_MAX_NESTED_SPREADS],
	PP_VA_g_sSkips[PP_VA_MAX_NESTED_SPREADS],
	PP_VA_g_sStacks[PP_VA_MAX_NESTED_SPREADS],
	PP_VA_g_sContexts[PP_VA_MAX_NESTED_SPREADS][AsmContext],
	PP_VA_g_sMaxNesting = 0,
	PP_VA_g_sPassthroughNestings = 0,
	PP_VA_g_sDoPushAddr = 0;

#define CALL@PP_VA_DoPush%8() PP_VA_DoPush%8(-1, -1, -1)

stock PP_VA_DoPush(const skippedBytes, const pushedBytes, const pushRequirements)
{

	#pragma unused pushedBytes, skippedBytes, pushRequirements
	static
		sReturnAddress = 0,
		sPushedParameters = 0,
		sParametersBase = 0,
		sTotalParameters = 0;

	#emit POP.pri
	#emit SCTRL        5

	#emit POP.pri
	#emit MOVE.alt
	#emit STOR.pri     sReturnAddress
	#emit POP.pri
	#emit POP.alt
	#emit LCTRL        5
	#emit ADD.C        12
	#emit ADD
	#emit STOR.pri     sParametersBase
	#emit LOAD.S.pri   8
	#emit SUB
	#emit STOR.pri     sPushedParameters
	#emit POP.alt
	#emit ADD
	#emit STOR.pri     sTotalParameters
	#emit POP.alt

	#emit LOAD.pri     sTotalParameters
	#emit STOR.I

	#emit LCTRL        4
	#emit LOAD.alt     sPushedParameters
	#emit SUB
	#emit SCTRL        4

	#emit STACK        0
	#emit PUSH.C       1000000
	#emit PUSH         sPushedParameters
	#emit PUSH.C       0
	#emit PUSH         sParametersBase
	#emit PUSH.alt
	#emit PUSH.C       20
	#emit SYSREQ.C     memcpy
	#emit STACK        24

	#emit LOAD.pri     sReturnAddress
	#emit SCTRL        8
	#emit SCTRL        6
}

	#define CALL@PP_VA_DummyPush%8() PP_VA_DummyPush%8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1)

stock PP_VA_DummyPush(const skippedBytes = 0, const pushedBytes = 0, const pushRequirements = cellbytes, const dummy0 = 0, const dummy1 = 0, const dummy2 = 0, const dummy3 = 0, const dummy4 = 0, const dummy5 = 0, const pad0=0, const pad1=0, const pad2=0, const pad3=0, const pad4=0, const pad5=0 )
{

	#pragma unused dummy0, dummy1, dummy2, dummy3, dummy4, dummy5, pad0, pad1, pad2, pad3, pad4, pad5

    #pragma unused skippedBytes, pushedBytes, pushRequirements
    print("[pp-va] Unsupported spread call: use a constant skip count and compiler -O0 or -O1.");
    #emit HALT 10
    return 0;
}

forward _pp@on_init@PPVASpread();
public _pp@on_init@PPVASpread()
{
    PP_VA_g_sDoPushAddr = addressof(PP_VA_DoPush);
    new context[DisasmContext];
    DisasmInit(context);
    PP_VA_Initialize();
    return 1;
}

stock PP_VA_Address(&value)
{
    new address;
    #emit LOAD.S.pri value
    #emit STOR.S.pri address
    return address;
}

#if !defined PP_VA_MAX_SPREAD_FRAMES
    #define PP_VA_MAX_SPREAD_FRAMES (128)
#endif
#if PP_VA_MAX_SPREAD_FRAMES < 1
    #error PP_VA_MAX_SPREAD_FRAMES must be positive.
#endif
static PP_VA_FrameDepth = -1;
static PP_VA_FrameOwners[PP_VA_MAX_SPREAD_FRAMES];
static PP_VA_CountFrames[PP_VA_MAX_SPREAD_FRAMES][PP_VA_MAX_NESTED_SPREADS];
#define CALL@PP_VA_CountSlot%8() PP_VA_CountSlot%8(0,0)
stock PP_VA_CountSlot(index, skippedBytes)
{
    new callerBytes, callerFrame;
    #emit LOAD.S.pri 0
    #emit STOR.S.pri callerFrame
    #emit ADD.C 8
    #emit LOAD.I
    #emit STOR.S.pri callerBytes
    if (skippedBytes < 0 || skippedBytes > callerBytes)
    {
        print("[pp-va] Spread skip count is outside the caller's argument range.");
        #emit HALT 10
    }
    if (!index)
    {
        while (PP_VA_FrameDepth >= 0 && PP_VA_FrameOwners[PP_VA_FrameDepth] <= callerFrame)
            --PP_VA_FrameDepth;
        ++PP_VA_FrameDepth;
    }
    if (PP_VA_FrameDepth < 0 || PP_VA_FrameDepth >= PP_VA_MAX_SPREAD_FRAMES)
    {
        print("[pp-va] Spread frame limit exceeded.");
        #emit HALT 10
    }
    PP_VA_FrameOwners[PP_VA_FrameDepth] = callerFrame;
    return PP_VA_Address(PP_VA_CountFrames[PP_VA_FrameDepth][index]);
}
static stock PP_VA_CodeGenPushSite(ctx[AsmContext], const pushedBytes, const skippedBytes, const index)
{
    @emit PUSH.C skippedBytes
    @emit PUSH.C index
    @emit PUSH.C 8
    @emit CALL.abs addressof(PP_VA_CountSlot)
    @emit PUSH.pri
    @emit PUSH.C pushedBytes - 4
    @emit PUSH.C skippedBytes
    @emit PUSH.C 12
    @emit CALL.abs PP_VA_g_sDoPushAddr
    @emit HEAP 4
}

static stock PP_VA_CodeGenShiftCode(dest, src, end)
{

	while (src != end)
	{
		#emit LREF.S.pri   src
		#emit SREF.S.pri   dest
		src += cellbytes;
		dest += cellbytes;
	}

	src = _:RelocateOpcode(OP_NOP);
	while (dest != end)
	{
		#emit LOAD.S.pri   src
		#emit SREF.S.pri   dest
		dest += cellbytes;
	}
}

stock PP_VA_CurrentSlot(index)
{
    return PP_VA_CountFrames[PP_VA_FrameDepth][index];
}
#define CALL@PP_VA_CurrentSlot%8() PP_VA_CurrentSlot%8(0)
stock PP_VA_EmitCount(ctx[AsmContext], index)
{
    @emit PUSH.C index
    @emit PUSH.C 4
    @emit CALL.abs addressof(PP_VA_CurrentSlot)
    @emit PUSH.pri
}

static stock PP_VA_CodeGenMainCleanup(ctx[AsmContext])
{
	static
		sTemp = 0;

	@emit POP.alt

	@emit STOR.pri     PP_VA_Address(sTemp)

	@emit LCTRL        4

	@emit ADD

	@emit SCTRL        4

	@emit LOAD.pri     PP_VA_Address(sTemp)
}

static stock PP_VA_FoundCall(const m[CodeScanner])
{
	if (PP_VA_g_sPassthroughNestings)
	{

		new
			pos = PP_VA_g_sPassthroughNestings - 1;

		if ((CodeScanGetMatchStack(m) < PP_VA_g_sStacks[pos]))
		{
			new
				len = CodeScanGetMatchLength(m),
				end = CodeScanGetMatchAddressData(m) + len,

				paramCount = CodeScanGetMatchHole(m, len == 12),
				codeLength = 76;
			PP_VA_g_sPassthroughNestings = pos;
			new
				dest = PP_VA_g_sContexts[pos][AsmContext_buffer];

			PP_VA_CodeGenPushSite(PP_VA_g_sContexts[pos], paramCount, PP_VA_g_sSkips[pos], pos),

			PP_VA_CodeGenShiftCode(
				dest + PP_VA_g_sContexts[pos][AsmContext_buffer_offset],
				dest + PP_VA_g_sLength[pos],
				end);

			PP_VA_g_sContexts[pos][AsmContext_buffer_offset] = (end - dest) - (PP_VA_g_sLength[pos] - codeLength) - len,
			dest = pos;
			if (len == 12)
			{

				end = CodeScanGetMatchHole(m, 0),
				PP_VA_EmitCount(PP_VA_g_sContexts[pos], dest),
				AsmEmitSysreqC(PP_VA_g_sContexts[pos], end);
			}
			else
			{

				PP_VA_EmitCount(PP_VA_g_sContexts[pos], dest);
                if (len == 16) AsmEmitCallAbs(PP_VA_g_sContexts[pos], CodeScanGetMatchHole(m, 1));
                else AsmEmitSysreqC(PP_VA_g_sContexts[pos], CodeScanGetMatchHole(m, 1));
			}
			if (len != 16)
				PP_VA_CodeGenMainCleanup(PP_VA_g_sContexts[pos]);
            if (!pos)
            {
                new ctx[AsmContext];
                ctx = PP_VA_g_sContexts[pos];
                @emit DEC PP_VA_Address(PP_VA_FrameDepth)
            }

			return 0;
		}
	}
	return -1;
}

static stock PP_VA_FoundPush(const m[CodeScanner])
{
	if (PP_VA_g_sPassthroughNestings >= PP_VA_MAX_NESTED_SPREADS)
	{
		print("[pp-va] Spread nesting limit exceeded.");
        #emit HALT 10
		return;
	}
	PP_VA_g_sMaxNesting = max(PP_VA_g_sMaxNesting, PP_VA_g_sPassthroughNestings),

	PP_VA_g_sLength[PP_VA_g_sPassthroughNestings] = CodeScanGetMatchLength(m),
	CodeScanGetMatchAsm(m, PP_VA_g_sContexts[PP_VA_g_sPassthroughNestings]),
	PP_VA_g_sStacks[PP_VA_g_sPassthroughNestings] = CodeScanGetMatchStack(m),

	PP_VA_g_sSkips[PP_VA_g_sPassthroughNestings] = (PP_VA_g_sLength[PP_VA_g_sPassthroughNestings] == 40 * cellbytes) ? 0 : CodeScanGetMatchHole(m, 0);
	++PP_VA_g_sPassthroughNestings;

}

static stock PP_VA_Initialize()
{

	new
		scanner[CodeScanner],
		csmO0A[CodeScanMatcher],
		csmO0B[CodeScanMatcher],
		csmO1[CodeScanMatcher],
		csm2[CodeScanMatcher],
		csm3[CodeScanMatcher];
	CodeScanInit(scanner);

	CodeScanMatcherInit(csmO0A, &PP_VA_FoundPush);
	CodeScanMatcherPattern(csmO0A,
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)

		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(CONST_PRI,  cellbytes)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(PUSH_C,     60)
		OP(CALL,       &PP_VA_DummyPush)
		OP(HEAP,       4)
		OP(STOR_I)
		OP(MOVE_PRI)
		OP(PUSH_PRI)
	);
	CodeScanAddMatcher(scanner, csmO0A);
	CodeScanMatcherInit(csmO0B, &PP_VA_FoundPush);
	CodeScanMatcherPattern(csmO0B,
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)
        OP(ZERO_PRI) OP(PUSH_PRI)

		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(CONST_PRI,  cellbytes)
		OP(PUSH_PRI)
		OP(ZERO_PRI)
		OP(PUSH_PRI)
		OP(CONST_PRI,  ???)
		OP(PUSH_PRI)
		OP(PUSH_C,     60)
		OP(CALL,       &PP_VA_DummyPush)
		OP(HEAP,       4)
		OP(STOR_I)
		OP(MOVE_PRI)
		OP(PUSH_PRI)
	);
    CodeScanAddMatcher(scanner, csmO0B);

	CodeScanMatcherInit(csmO1, &PP_VA_FoundPush);
	CodeScanMatcherPattern(csmO1,
        OP(PUSH_C, 0)
        OP(PUSH_C, 0)
        OP(PUSH_C, 0)
        OP(PUSH_C, 0)
        OP(PUSH_C, 0)
        OP(PUSH_C, 0)

		OP(PUSH_C,     0)
		OP(PUSH_C,     0)
		OP(PUSH_C,     0)
		OP(PUSH_C,     0)
		OP(PUSH_C,     0)
		OP(PUSH_C,     0)
		OP(PUSH_C,     cellbytes)
		OP(PUSH_C,     0)
		OP(PUSH_C,     ???)
		OP(PUSH_C,     60)
		OP(CALL,       &PP_VA_DummyPush)
		OP(HEAP,       4)
		OP(STOR_I)
		OP(PUSH_ALT)
	);
	CodeScanAddMatcher(scanner, csmO1);

	CodeScanMatcherInit(csm2, &PP_VA_FoundCall);
	CodeScanMatcherPattern(csm2,
		OP(PUSH_C,     ???)
		OP(CALL,       ???)
	);
	CodeScanAddMatcher(scanner, csm2);
	CodeScanMatcherInit(csm3, &PP_VA_FoundCall);

	CodeScanMatcherPattern(csm3,
		OP(PUSH_C,     ???)
		OP(SYSREQ_C,   ???)
		OP(STACK,      ???)
	);
	CodeScanAddMatcher(scanner, csm3);

	CodeScanRun(scanner);

	return 1;
}


#endif
