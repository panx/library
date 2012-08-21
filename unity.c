/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#include "unity.h"
#include <stdio.h>
#include <string.h>

#define UNITY_FAIL_AND_BAIL   { Unity.CurrentTestFailed  = 1; UNITY_OUTPUT_CHAR('\n',1); UNITY_OUTPUT_CHAR('\n',0);  longjmp(Unity.AbortFrame, 1); }
#define UNITY_IGNORE_AND_BAIL { Unity.CurrentTestIgnored = 1; UNITY_OUTPUT_CHAR('\n',1); UNITY_OUTPUT_CHAR('\n',0); longjmp(Unity.AbortFrame, 1); }
/// return prematurely if we are already in failure or ignore state
#define UNITY_SKIP_EXECUTION  { if ((Unity.CurrentTestFailed != 0) || (Unity.CurrentTestIgnored != 0)) {return;} }
#define UNITY_PRINT_EOL       { UNITY_OUTPUT_CHAR('\n',1); UNITY_OUTPUT_CHAR('\n',0);}

struct _Unity Unity = { 0 };

const char* UnityStrNull     = "NULL";
const char* UnityStrSpacer   = ". ";
const char* UnityStrExpected[2] = {" \"Message\":\"Expected ","Expected"};
const char* UnityStrWas      = " Was ";
const char* UnityStrTo       = " To ";
const char* UnityStrElement  = " Element ";
const char* UnityStrByte     = " Byte ";
const char* UnityStrMemory   = " Memory Mismatch.";
const char* UnityStrDelta    = " Values Not Within Delta ";
const char* UnityStrPointless= " You Asked Me To Compare Nothing, Which Was Pointless.";
const char* UnityStrNullPointerForExpected= " Expected pointer to be NULL";
const char* UnityStrNullPointerForActual  = " Actual pointer was NULL";

// compiler-generic print formatting masks
const _U_UINT UnitySizeMask[] = 
{
    255u,         // 0xFF
    65535u,       // 0xFFFF
    65535u,
    4294967295u,  // 0xFFFFFFFF
    4294967295u,
    4294967295u,
    4294967295u
#ifdef UNITY_SUPPORT_64
    ,0xFFFFFFFFFFFFFFFF
#endif
};

void UnityPrintFail(void);
void UnityPrintOk(void);

int put_char(int a, _US32 index){
    FILE* fp;
    if (index==1) {
        putchar(a);
    }
    else
    {
        char filename[256]="";
        strcat(filename,Unity.TestFile);
        strcat(filename,".json");
        fp = fopen(filename,"a");
        fputc( a, fp);
        fclose(fp);
    }
    return 0;
}
//-----------------------------------------------
// Pretty Printers & Test Result Output Handlers
//-----------------------------------------------

void UnityPrint(const char* string, _US32 index)
{
    const char* pch = string;

    if (pch != NULL)
    {
        while (*pch)
        {
            // printable characters plus CR & LF are printed
            if ((*pch <= 126) && (*pch >= 32))
            {
                UNITY_OUTPUT_CHAR(*pch, index);
            }
            //write escaped carriage returns
            else if (*pch == 13)
            {
                UNITY_OUTPUT_CHAR('\\', index);
                UNITY_OUTPUT_CHAR('r', index);
            }
            //write escaped line feeds
            else if (*pch == 10)
            {
                UNITY_OUTPUT_CHAR('\\',index);
                UNITY_OUTPUT_CHAR('n',index);
            }
            // unprintable characters are shown as codes
            else
            {
                UNITY_OUTPUT_CHAR('\\',index);
                UnityPrintNumberHex((_U_SINT)*pch, 2, index);
            }
            pch++;
        }
    }
}

//-----------------------------------------------
void UnityPrintNumberByStyle(const _U_SINT number, const UNITY_DISPLAY_STYLE_T style, _US32 index)
{
    if ((style & UNITY_DISPLAY_RANGE_INT) == UNITY_DISPLAY_RANGE_INT)
    {
        UnityPrintNumber(number,index);
    }
    else if ((style & UNITY_DISPLAY_RANGE_UINT) == UNITY_DISPLAY_RANGE_UINT)
    {
        UnityPrintNumberUnsigned(  (_U_UINT)number  &  UnitySizeMask[((_U_UINT)style & (_U_UINT)0x0F) - 1], index  );
    }
    else
    {
        UnityPrintNumberHex((_U_UINT)number, (style & 0x000F) << 1, index);
    }
}

//-----------------------------------------------
/// basically do an itoa using as little ram as possible
void UnityPrintNumber(const _U_SINT number_to_print, _US32 index)
{
    _U_SINT divisor = 1;
    _U_SINT next_divisor;
    _U_SINT number = number_to_print;

    if (number < 0)
    {
        UNITY_OUTPUT_CHAR('-',index);
        number = -number;
    }

    // figure out initial divisor
    while (number / divisor > 9)
    {
        next_divisor = divisor * 10;
        if (next_divisor > divisor)
            divisor = next_divisor;
        else
            break;
    }

    // now mod and print, then divide divisor
    do
    {
        UNITY_OUTPUT_CHAR((char)('0' + (number / divisor % 10)),index);
        divisor /= 10;
    }
    while (divisor > 0);
}

//-----------------------------------------------
/// basically do an itoa using as little ram as possible
void UnityPrintNumberUnsigned(const _U_UINT number,_US32 index)
{
    _U_UINT divisor = 1;
    _U_UINT next_divisor;

    // figure out initial divisor
    while (number / divisor > 9)
    {
        next_divisor = divisor * 10;
        if (next_divisor > divisor)
            divisor = next_divisor;
        else
            break;
    }

    // now mod and print, then divide divisor
    do
    {
        UNITY_OUTPUT_CHAR((char)('0' + (number / divisor % 10)), index);
        divisor /= 10;
    }
    while (divisor > 0);
}

//-----------------------------------------------
void UnityPrintNumberHex(const _U_UINT number, const char nibbles_to_print, _US32 index)
{
    _U_UINT nibble;
    char nibbles = nibbles_to_print;
    UNITY_OUTPUT_CHAR('0',index);
    UNITY_OUTPUT_CHAR('x',index);

    while (nibbles > 0)
    {
        nibble = (number >> (--nibbles << 2)) & 0x0000000F;
        if (nibble <= 9)
        {
            UNITY_OUTPUT_CHAR((char)('0' + nibble),index);
        }
        else
        {
            UNITY_OUTPUT_CHAR((char)('A' - 10 + nibble), index);
        }
    }
}

//-----------------------------------------------
void UnityPrintMask(const _U_UINT mask, const _U_UINT number, _US32 index)
{
    _U_UINT current_bit = (_U_UINT)1 << (UNITY_INT_WIDTH - 1);
    _US32 i;
    for (i = 0; i < UNITY_INT_WIDTH; i++)
    {
        if (current_bit & mask)
        {
            if (current_bit & number)
            {
                UNITY_OUTPUT_CHAR('1', index);
            }
            else
            {
                UNITY_OUTPUT_CHAR('0', index);
            }
        }
        else
        {
            UNITY_OUTPUT_CHAR('X',index);
        }
        current_bit = current_bit >> 1;
    }
}

//-----------------------------------------------
#ifdef UNITY_FLOAT_VERBOSE
void UnityPrintFloat(_UF number, _US32 index)
{
    char TempBuffer[32];
    sprintf(TempBuffer, "%.6f", number);
    UnityPrint(TempBuffer,index);
}
#endif

//-----------------------------------------------

void UnityPrintFail(void)
{
    UnityPrint("FAIL",1);
}

void UnityPrintOk(void)
{
    UnityPrint("OK",1);
}

//-----------------------------------------------
void UnityTestResultsBegin(const char* file, const UNITY_LINE_TYPE line, _US32 index)
{
    if (index==1) {
        UnityPrint(file, index);
        UNITY_OUTPUT_CHAR(':', index);
        UnityPrintNumber(line, index);
        UNITY_OUTPUT_CHAR(':', index);
        UnityPrint(Unity.CurrentTestName, index);
        UNITY_OUTPUT_CHAR(':', index);
    }
    else{
        
        UNITY_OUTPUT_CHAR('{', index);
        UNITY_OUTPUT_CHAR('\n', index);
        UnityPrint("\"filename\":", index);
        UNITY_OUTPUT_CHAR('\"',index);
        UnityPrint(file,index);
        UNITY_OUTPUT_CHAR('\"',index);
        UNITY_OUTPUT_CHAR(',',index);
        UNITY_OUTPUT_CHAR('\n',index);
        
        UnityPrint("\"functionn_name\":",index);
        UNITY_OUTPUT_CHAR('\"',index);
        UnityPrint(Unity.CurrentTestName,index);
        UNITY_OUTPUT_CHAR('\"',index);
        UNITY_OUTPUT_CHAR(',',index);
        UNITY_OUTPUT_CHAR('\n',index);
        
        UnityPrint("\"linenumber\":",index);
        UNITY_OUTPUT_CHAR('\"',index);
        UnityPrintNumber(Unity.CurrentTestLineNumber,index);
        UNITY_OUTPUT_CHAR('\"',index);
        UNITY_OUTPUT_CHAR(',',index);
        UNITY_OUTPUT_CHAR('\n',index);
    }
        

}

//-----------------------------------------------
void UnityTestResultsFailBegin(const UNITY_LINE_TYPE line, _US32 index)
{
    UnityTestResultsBegin(Unity.TestFile, line, index);
    if (index==1) {
        UnityPrint("FAIL:", index);
    }
    else{
        UnityPrint("\"IGNORE\":\"NOT IGNORED\"",index);
        UNITY_OUTPUT_CHAR(',',index);
        UNITY_OUTPUT_CHAR('\n',index);
        UnityPrint("\"result\":\"Fail\"",index);
        UNITY_OUTPUT_CHAR(',',index);
        UNITY_OUTPUT_CHAR('\n',index);
    }
    
}

//-----------------------------------------------
void UnityConcludeTest()
{
    _US32 index;
    if (Unity.CurrentTestIgnored)
    {
        Unity.TestIgnores++;
    }
    else if (!Unity.CurrentTestFailed)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsBegin(Unity.TestFile, Unity.CurrentTestLineNumber, index);
            if (index) {
                UnityPrint("PASS",index);
            }
            else{
                UnityPrint("\"ignored\":\"NO\"",index);
                UNITY_OUTPUT_CHAR(',',index);
                UNITY_OUTPUT_CHAR('\n',index);
                UnityPrint("\"result\":\"PASS\"",index);
                UNITY_OUTPUT_CHAR(',',index);
                UNITY_OUTPUT_CHAR('\n',index);
                UnityPrint("\"message\":\"\"",index);
                UNITY_OUTPUT_CHAR('\n',index);
                UNITY_OUTPUT_CHAR('}',index);
            }
        }

        
        UNITY_PRINT_EOL;
    }
    else
    {
        Unity.TestFailures++;
    }

    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;
}

//-----------------------------------------------
void UnityAddMsgIfSpecified(const char* msg, _US32 index)
{
    if (msg)
    {
        UnityPrint(UnityStrSpacer,index);
        UnityPrint(msg,index);
    }
}

//-----------------------------------------------
void UnityPrintExpectedAndActualStrings(const char* expected, const char* actual)
{
    _US32 index;
    
    if (expected != NULL)
    {
        index=0;
        UnityPrint(UnityStrExpected[index],index);
        UNITY_OUTPUT_CHAR('\'',0);
        UnityPrint(expected,index);
        index=1;
        UnityPrint(expected,index);
        UNITY_OUTPUT_CHAR('\'',0);
    }
    else
    {
      UnityPrint(UnityStrNull,1);
      UnityPrint(UnityStrNull,0);
    }
    for (index=0; index<2; index++) {
        UnityPrint(UnityStrWas,index);
        if (actual != NULL)
        {
            UNITY_OUTPUT_CHAR('\'',index);
            UnityPrint(actual,index);
            UNITY_OUTPUT_CHAR('\'',index);
        }
        else
        {
            UnityPrint(UnityStrNull,index);          
        }
    }
    UNITY_OUTPUT_CHAR('\"',0);
    UNITY_OUTPUT_CHAR('\n',0);
    UNITY_OUTPUT_CHAR('}',0);

}

//-----------------------------------------------
// Assertion & Control Helpers
//-----------------------------------------------

int UnityCheckArraysForNull(const void* expected, const void* actual, const UNITY_LINE_TYPE lineNumber, const char* msg)
{
    
    _US32 index;
    
    //return true if they are both NULL
    if ((expected == NULL) && (actual == NULL))
        return 1;
        
    //throw error if just expected is NULL
    if (expected == NULL)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrNullPointerForExpected,index);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_FAIL_AND_BAIL;
    }

    //throw error if just actual is NULL
    if (actual == NULL)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrNullPointerForActual,index);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_FAIL_AND_BAIL;

    }
    
    //return false if neither is NULL
    return 0;
}

//-----------------------------------------------
// Assertion Functions
//-----------------------------------------------

void UnityAssertBits(const _U_SINT mask,
                     const _U_SINT expected,
                     const _U_SINT actual,
                     const char* msg,
                     const UNITY_LINE_TYPE lineNumber)
{
    UNITY_SKIP_EXECUTION;
    _US32 index;
    if ((mask & expected) != (mask & actual))
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrExpected[index],index);
            UnityPrintMask(mask, expected,index);
            UnityPrint(UnityStrWas,index);
            UnityPrintMask(mask, actual,index);
            
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_OUTPUT_CHAR('\"',0);
        UNITY_OUTPUT_CHAR('\n',0);
        UNITY_OUTPUT_CHAR('}',0);
        UNITY_FAIL_AND_BAIL;
    }
}

//-----------------------------------------------
void UnityAssertEqualNumber(const _U_SINT expected,
                            const _U_SINT actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber,
                            const UNITY_DISPLAY_STYLE_T style)
{
    UNITY_SKIP_EXECUTION;
    _US32 index;
    if (expected != actual)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrExpected[index],index);
            UnityPrintNumberByStyle(expected, style,index);
            UnityPrint(UnityStrWas,index);
            UnityPrintNumberByStyle(actual, style,index);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_OUTPUT_CHAR('\"',0);
        UNITY_OUTPUT_CHAR('\n',0);
        UNITY_OUTPUT_CHAR('}',0);
        UNITY_FAIL_AND_BAIL;
    }
}

//-----------------------------------------------
void UnityAssertEqualIntArray(const _U_SINT* expected,
                              const _U_SINT* actual,
                              const _UU32 num_elements,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const UNITY_DISPLAY_STYLE_T style)
{
    _UU32 elements = num_elements;
    const _US8* ptr_exp = (_US8*)expected;
    const _US8* ptr_act = (_US8*)actual;

    UNITY_SKIP_EXECUTION;
    _US32 index;
    if (elements == 0)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrPointless,index);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_FAIL_AND_BAIL;
    }
    
    if (UnityCheckArraysForNull((void*)expected, (void*)actual, lineNumber, msg) == 1)
        return;

    switch(style)
    {
        case UNITY_DISPLAY_STYLE_HEX8:
        case UNITY_DISPLAY_STYLE_INT8:
        case UNITY_DISPLAY_STYLE_UINT8:
            while (elements--)
            {
                if (*ptr_exp != *ptr_act)
                {
                    for (index=0; index<2; index++) {
                        UnityTestResultsFailBegin(lineNumber,index);
                        UnityPrint(UnityStrElement,index);
                        UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT,index);
                        UnityPrint(UnityStrExpected[index],index);
                        UnityPrintNumberByStyle(*ptr_exp, style,index);
                        UnityPrint(UnityStrWas,index);
                        UnityPrintNumberByStyle(*ptr_act, style,index);
                        UnityAddMsgIfSpecified(msg,index);
                    }
                    UNITY_OUTPUT_CHAR('\"',0);
                    UNITY_OUTPUT_CHAR('\n',0);
                    UNITY_OUTPUT_CHAR('}',0);
                    UNITY_FAIL_AND_BAIL;
                    
                }
                ptr_exp += 1;
                ptr_act += 1;
            }
            break;
        case UNITY_DISPLAY_STYLE_HEX16:
        case UNITY_DISPLAY_STYLE_INT16:
        case UNITY_DISPLAY_STYLE_UINT16:
            while (elements--)
            {
                if (*(_US16*)ptr_exp != *(_US16*)ptr_act)
                {
                    for (index=0; index<2; index++) {
                        UnityTestResultsFailBegin(lineNumber,index);
                        UnityPrint(UnityStrElement,index);
                        UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT,index);
                        UnityPrint(UnityStrExpected[index],index);
                        UnityPrintNumberByStyle(*(_US16*)ptr_exp, style,index);
                        UnityPrint(UnityStrWas,index);
                        UnityPrintNumberByStyle(*(_US16*)ptr_act, style,index);
                        UnityAddMsgIfSpecified(msg,index);
                    }
                    UNITY_OUTPUT_CHAR('\"',0);
                    UNITY_OUTPUT_CHAR('\n',0);
                    UNITY_OUTPUT_CHAR('}',0);
                    UNITY_FAIL_AND_BAIL;
                }
                ptr_exp += 2;
                ptr_act += 2;
            }
            break;
#ifdef UNITY_SUPPORT_64
        case UNITY_DISPLAY_STYLE_HEX64:
        case UNITY_DISPLAY_STYLE_INT64:
        case UNITY_DISPLAY_STYLE_UINT64:
            while (elements--)
            {
                if (*(_US64*)ptr_exp != *(_US64*)ptr_act)
                {
                    for ( index=0; index<2; index++) {
                        UnityTestResultsFailBegin(lineNumber,index);
                        UnityPrint(UnityStrElement,index);
                        UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT,index);
                        UnityPrint(UnityStrExpected[index],index);
                        UnityPrintNumberByStyle(*(_US64*)ptr_exp, style,index);
                        UnityPrint(UnityStrWas,index);
                        UnityPrintNumberByStyle(*(_US64*)ptr_act, style,index);
                        UnityAddMsgIfSpecified(msg,index);
                    }
                    UNITY_OUTPUT_CHAR('\"',0);
                    UNITY_OUTPUT_CHAR('\n',0);
                    UNITY_OUTPUT_CHAR('}',0);
                    UNITY_FAIL_AND_BAIL;
                }
                ptr_exp += 8;
                ptr_act += 8;
            }
            break;
#endif
        default:
            while (elements--)
            {
                if (*(_US32*)ptr_exp != *(_US32*)ptr_act)
                {        
                    for (index=0; index<2; index++) {
                        UnityTestResultsFailBegin(lineNumber,index);
                        UnityPrint(UnityStrElement,index);
                        UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT,index);
                        UnityPrint(UnityStrExpected[index],index);
                        UnityPrintNumberByStyle(*(_US32*)ptr_exp, style,index);
                        UnityPrint(UnityStrWas,index);
                        UnityPrintNumberByStyle(*(_US32*)ptr_act, style,index);
                        UnityAddMsgIfSpecified(msg,index);
                    }
                    UNITY_OUTPUT_CHAR('\"',0);
                    UNITY_OUTPUT_CHAR('\n',0);
                    UNITY_OUTPUT_CHAR('}',0);
                    UNITY_FAIL_AND_BAIL;
                }
                ptr_exp += 4;
                ptr_act += 4;
            }
            break;
    }
}

//-----------------------------------------------
#ifndef UNITY_EXCLUDE_FLOAT
void UnityAssertEqualFloatArray(const _UF* expected,
                                const _UF* actual,
                                const _UU32 num_elements,
                                const char* msg,
                                const UNITY_LINE_TYPE lineNumber)
{
    _UU32 elements = num_elements;
    const _UF* ptr_expected = expected;
    const _UF* ptr_actual = actual;
    _UF diff, tol;
    _US32 index;
    UNITY_SKIP_EXECUTION;
  
    if (elements == 0)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrPointless,index);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_FAIL_AND_BAIL;
    }
    
    if (UnityCheckArraysForNull((void*)expected, (void*)actual, lineNumber, msg) == 1)
        return;

    while (elements--)
    {
        diff = *ptr_expected - *ptr_actual;
        if (diff < 0.0)
          diff = 0.0 - diff;
        tol = UNITY_FLOAT_PRECISION * *ptr_expected;
        if (tol < 0.0)
            tol = 0.0 - tol;
        if (diff > tol)
        {
            for (index=0; index<2; index++) {
                UnityTestResultsFailBegin(lineNumber,index);
                UnityPrint(UnityStrElement,index);
                UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT,index);
#ifdef UNITY_FLOAT_VERBOSE
                UnityPrint(UnityStrExpected[index],index);
                UnityPrintFloat(*ptr_expected,index);
                UnityPrint(UnityStrWas,index);
                UnityPrintFloat(*ptr_actual,index);
#else
                UnityPrint(UnityStrDelta,index);
#endif
                UnityAddMsgIfSpecified(msg,index);
            }
            UNITY_OUTPUT_CHAR('\"',0);
            UNITY_OUTPUT_CHAR('\n',0);
            UNITY_OUTPUT_CHAR('}',0);
            UNITY_FAIL_AND_BAIL;
        }
        ptr_expected++;
        ptr_actual++;
    }
}

//-----------------------------------------------
void UnityAssertFloatsWithin(const _UF delta,
                             const _UF expected,
                             const _UF actual,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber)
{
    _UF diff = actual - expected;
    _UF pos_delta = delta;
    _US32 index;
    UNITY_SKIP_EXECUTION;
  
    if (diff < 0)
    {
        diff = 0.0f - diff;
    }
    if (pos_delta < 0)
    {
        pos_delta = 0.0f - pos_delta;
    }

    if (pos_delta < diff)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
#ifdef UNITY_FLOAT_VERBOSE
            UnityPrint(UnityStrExpected[index],index);
            UnityPrintFloat(expected,index);
            UnityPrint(UnityStrWas,index);
            UnityPrintFloat(actual,index);
#else
            UnityPrint(UnityStrDelta,index);
#endif
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_OUTPUT_CHAR('\"',0);
        UNITY_OUTPUT_CHAR('\n',0);
        UNITY_OUTPUT_CHAR('}',0);
        UNITY_FAIL_AND_BAIL;
    }
}

#endif //not UNITY_EXCLUDE_FLOAT

//-----------------------------------------------
#ifndef UNITY_EXCLUDE_DOUBLE
void UnityAssertEqualDoubleArray(const _UD* expected,
                                 const _UD* actual,
                                 const _UU32 num_elements,
                                 const char* msg,
                                 const UNITY_LINE_TYPE lineNumber)
{
    _UU32 elements = num_elements;
    const _UD* ptr_expected = expected;
    const _UD* ptr_actual = actual;
    _UD diff, tol;
    int index;
    UNITY_SKIP_EXECUTION;
  
    if (elements == 0)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrPointless,index);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_FAIL_AND_BAIL;
    }
    
    if (UnityCheckArraysForNull((void*)expected, (void*)actual, lineNumber, msg) == 1)
        return;

    while (elements--)
    {
        diff = *ptr_expected - *ptr_actual;
        if (diff < 0.0)
          diff = 0.0 - diff;
        tol = UNITY_DOUBLE_PRECISION * *ptr_expected;
        if (tol < 0.0)
            tol = 0.0 - tol;
        if (diff > tol)
        {
            for (index=0; index<2; index++) {
                UnityTestResultsFailBegin(lineNumber,index);
                UnityPrint(UnityStrElement,index);
                UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT,index);
#ifdef UNITY_DOUBLE_VERBOSE
                UnityPrint(UnityStrExpected[index],index);
                UnityPrintFloat((float)(*ptr_expected),index);
                UnityPrint(UnityStrWas,index);
                UnityPrintFloat((float)(*ptr_actual),index);
#else
                UnityPrint(UnityStrDelta,index);
#endif
                UnityAddMsgIfSpecified(msg,index);
            }
            UNITY_OUTPUT_CHAR('\"',0);
            UNITY_OUTPUT_CHAR('\n',0);
            UNITY_OUTPUT_CHAR('}',0);
            UNITY_FAIL_AND_BAIL;
        }
        ptr_expected++;
        ptr_actual++;
    }
}

//-----------------------------------------------
void UnityAssertDoublesWithin(const _UD delta,
                              const _UD expected,
                              const _UD actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber)
{
    _UD diff = actual - expected;
    _UD pos_delta = delta;
    _US32 index;
    UNITY_SKIP_EXECUTION;
  
    if (diff < 0)
    {
        diff = 0.0f - diff;
    }
    if (pos_delta < 0)
    {
        pos_delta = 0.0f - pos_delta;
    }

    if (pos_delta < diff)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
#ifdef UNITY_DOUBLE_VERBOSE
            UnityPrint(UnityStrExpected[index],index);
            UnityPrintFloat((float)expected,index);
            UnityPrint(UnityStrWas,index);
            UnityPrintFloat((float)actual,index);
#else
            UnityPrint(UnityStrDelta,index);
#endif
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_OUTPUT_CHAR('\"',0);
        UNITY_OUTPUT_CHAR('\n',0);
        UNITY_OUTPUT_CHAR('}',0);
        UNITY_FAIL_AND_BAIL;
    }
}

#endif // not UNITY_EXCLUDE_DOUBLE

//-----------------------------------------------
void UnityAssertNumbersWithin( const _U_SINT delta,
                               const _U_SINT expected,
                               const _U_SINT actual,
                               const char* msg,
                               const UNITY_LINE_TYPE lineNumber,
                               const UNITY_DISPLAY_STYLE_T style)
{
    UNITY_SKIP_EXECUTION;
    _US32 index;    
    if ((style & UNITY_DISPLAY_RANGE_INT) == UNITY_DISPLAY_RANGE_INT)
    {
        if (actual > expected)
          Unity.CurrentTestFailed = ((actual - expected) > delta);
        else
          Unity.CurrentTestFailed = ((expected - actual) > delta);
    }
    else
    {
        if ((_U_UINT)actual > (_U_UINT)expected)
            Unity.CurrentTestFailed = ((_U_UINT)(actual - expected) > (_U_UINT)delta);
        else
            Unity.CurrentTestFailed = ((_U_UINT)(expected - actual) > (_U_UINT)delta);
    }

    if (Unity.CurrentTestFailed)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrDelta,index);
            UnityPrintNumberByStyle(delta, style,index);
            UnityPrint(UnityStrExpected[index],index);
            UnityPrintNumberByStyle(expected, style,index);
            UnityPrint(UnityStrWas,index);
            UnityPrintNumberByStyle(actual, style,index);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_OUTPUT_CHAR('\"',0);
        UNITY_OUTPUT_CHAR('\n',0);
        UNITY_OUTPUT_CHAR('}',0);
        UNITY_FAIL_AND_BAIL;
    }
}

//-----------------------------------------------
void UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber)
{
    _UU32 i;
    _US32 index;
    UNITY_SKIP_EXECUTION;
  
    // if both pointers not null compare the strings
    if (expected && actual)
    {
        for (i = 0; expected[i] || actual[i]; i++)
        {
            if (expected[i] != actual[i])
            {
                Unity.CurrentTestFailed = 1;
                break;
            }
        }
    }
    else
    { // handle case of one pointers being null (if both null, test should pass)
        if (expected != actual)
        {
            Unity.CurrentTestFailed = 1;
        }
    }

    if (Unity.CurrentTestFailed)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrintExpectedAndActualStrings(expected, actual);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_OUTPUT_CHAR('\"',0);
        UNITY_OUTPUT_CHAR('}',0);
        UNITY_FAIL_AND_BAIL;
    }
}

//-----------------------------------------------
void UnityAssertEqualStringArray( const char** expected,
                                  const char** actual,
                                  const _UU32 num_elements,
                                  const char* msg,
                                  const UNITY_LINE_TYPE lineNumber)
{
    _UU32 i, j = 0;
    _US32 index;    
    UNITY_SKIP_EXECUTION;
  
    // if no elements, it's an error
    if (num_elements == 0)
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrPointless,index);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_FAIL_AND_BAIL;
    }

    if (UnityCheckArraysForNull((void*)expected, (void*)actual, lineNumber, msg) == 1)
        return;
    
    do
    {
        // if both pointers not null compare the strings
        if (expected[j] && actual[j])
        {
            for (i = 0; expected[j][i] || actual[j][i]; i++)
            {
                if (expected[j][i] != actual[j][i])
                {
                    Unity.CurrentTestFailed = 1;
                    break;
                }
            }
        }
        else
        { // handle case of one pointers being null (if both null, test should pass)
            if (expected[j] != actual[j])
            {
                Unity.CurrentTestFailed = 1;
            }
        }

        if (Unity.CurrentTestFailed)
        {
            for (index=0; index<2; index++) {
                UnityTestResultsFailBegin(lineNumber,index);
                if (num_elements > 1)
                {
                    UnityPrint(UnityStrElement,index);
                    UnityPrintNumberByStyle((num_elements - j - 1), UNITY_DISPLAY_STYLE_UINT,index);
                }
                UnityPrintExpectedAndActualStrings((const char*)(expected[j]), (const char*)(actual[j]));
                UnityAddMsgIfSpecified(msg,index);
            }
            UNITY_FAIL_AND_BAIL;
        } 
    } while (++j < num_elements);
}

//-----------------------------------------------
void UnityAssertEqualMemory( const void* expected,
                             const void* actual,
                             const _UU32 length,
                             const _UU32 num_elements,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber)
{
    unsigned char* ptr_exp = (unsigned char*)expected;
    unsigned char* ptr_act = (unsigned char*)actual;
    _UU32 elements = num_elements;
    _UU32 bytes;
    _US32 index;
    UNITY_SKIP_EXECUTION;
  
    if ((elements == 0) || (length == 0))
    {
        for (index=0; index<2; index++) {
            UnityTestResultsFailBegin(lineNumber,index);
            UnityPrint(UnityStrPointless,index);
            UnityAddMsgIfSpecified(msg,index);
        }
        UNITY_FAIL_AND_BAIL;
    }

    if (UnityCheckArraysForNull((void*)expected, (void*)actual, lineNumber, msg) == 1)
        return;
        
    while (elements--)
    {
        /////////////////////////////////////
        bytes = length;
        while (bytes--)
        {
            if (*ptr_exp != *ptr_act)
            {
                for (index=0; index<2; index++) {
                    UnityTestResultsFailBegin(lineNumber,index);
                    UnityPrint(UnityStrMemory,index);
                    if (num_elements > 1)
                    {
                        UnityPrint(UnityStrElement,index);
                        UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT,index);
                    }
                    UnityPrint(UnityStrByte,index);
                    UnityPrintNumberByStyle((length - bytes - 1), UNITY_DISPLAY_STYLE_UINT,index);
                    UnityPrint(UnityStrExpected[index],index);
                    UnityPrintNumberByStyle(*ptr_exp, UNITY_DISPLAY_STYLE_HEX8,index);
                    UnityPrint(UnityStrWas,index);
                    UnityPrintNumberByStyle(*ptr_act, UNITY_DISPLAY_STYLE_HEX8,index);
                    UnityAddMsgIfSpecified(msg,index);
                }
                UNITY_OUTPUT_CHAR('\"',0);
                UNITY_OUTPUT_CHAR('\n',0);
                UNITY_OUTPUT_CHAR('}',0);
                UNITY_FAIL_AND_BAIL;
            }
            ptr_exp += 1;
            ptr_act += 1;
        }
        /////////////////////////////////////
        
    }
}

//-----------------------------------------------
// Control Functions
//-----------------------------------------------

void UnityFail(const char* msg, const UNITY_LINE_TYPE line)
{
    UNITY_SKIP_EXECUTION;
    _US32 index;
    for (index=0; index<2; index++) {
        UnityTestResultsBegin(Unity.TestFile, line, index);
        UnityPrintFail();
        if (msg != NULL)
        {
            UNITY_OUTPUT_CHAR(':',index);
            if (msg[0] != ' ')
            {
                UNITY_OUTPUT_CHAR(' ',index);  
            }
            UnityPrint(msg,index);
        }
    }
    UNITY_FAIL_AND_BAIL;
}

//-----------------------------------------------
void UnityIgnore(const char* msg, const UNITY_LINE_TYPE line)
{
    UNITY_SKIP_EXECUTION;
    _US32 index;
    for (index=0; index<2; index++) {
        UnityTestResultsBegin(Unity.TestFile, line, index);
        if (index==1) {
            UnityPrint("IGNORE",index);
            if (msg != NULL)
            {
                UNITY_OUTPUT_CHAR(':',index);
                UNITY_OUTPUT_CHAR(' ',index);
                UnityPrint(msg,index);
            }
        }
        else if(index==0){
            UNITY_OUTPUT_CHAR('\"',index);
            UnityPrint("IGNORE",index);
            UNITY_OUTPUT_CHAR('\"',index);
            UNITY_OUTPUT_CHAR(':',index);
            UNITY_OUTPUT_CHAR(' ',index);
            UNITY_OUTPUT_CHAR('\"',index);
            if (msg != NULL)
            {
                UnityPrint(msg,index); 
            }
            UNITY_OUTPUT_CHAR('\"',index);
            UNITY_OUTPUT_CHAR('\n',index);
            UnityPrint("\"Result\":\"\"",index);
            UNITY_OUTPUT_CHAR('\n',index);
            UnityPrint("\"Message\":\"\"",index);
            UNITY_OUTPUT_CHAR('\n',index);
            UNITY_OUTPUT_CHAR('}',index); 
        }
    }
    UNITY_IGNORE_AND_BAIL;
}

//-----------------------------------------------
void setUp(void);
void tearDown(void);
void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum)
{
    Unity.CurrentTestName = FuncName;
    Unity.CurrentTestLineNumber = FuncLineNum;
    Unity.NumberOfTests++; 
    if (TEST_PROTECT())
    {
        setUp();
        Func();
    }
    if (TEST_PROTECT() && !(Unity.CurrentTestIgnored))
    {
        tearDown();
    }
    UnityConcludeTest();
}

//-----------------------------------------------
void UnityBegin(void)
{
    Unity.NumberOfTests = 0;
    Unity.TestFailures = 0;
    Unity.TestIgnores = 0;
    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;
}

//-----------------------------------------------
int UnityEnd(void)
{
    _US32 index=1;
    UnityPrint("-----------------------",index);
    UNITY_PRINT_EOL;
    UnityPrintNumber(Unity.NumberOfTests,index);
    UnityPrint(" Tests ",index);
    UnityPrintNumber(Unity.TestFailures,index);
    UnityPrint(" Failures ",index);
    UnityPrintNumber(Unity.TestIgnores,index);
    UnityPrint(" Ignored",index);
    UNITY_PRINT_EOL;
    if (Unity.TestFailures == 0U)
    {
        UnityPrintOk();
    }
    else
    {
        UnityPrintFail();
    }
    UNITY_PRINT_EOL;
    return Unity.TestFailures;
}
