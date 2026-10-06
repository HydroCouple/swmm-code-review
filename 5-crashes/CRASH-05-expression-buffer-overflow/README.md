# CRASH-05: A long number or name in an expression overflows a 255-byte buffer

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | A treatment, groundwater-flow or control-rule expression containing a numeric constant or a name of 255 characters or more writes past the end of a fixed buffer while the input file is read: a stack buffer for numbers, a global for names. The result is memory corruption at input time, with no error message; under AddressSanitizer the run stops at once. The inputs are unusual (a pasted run of digits, a generated name), but the write is unbounded and reachable from a plain input file. |
| **Reached from** | `[TREATMENT]`, `[GWF]` and `[CONTROLS]` `EXPRESSION` lines (up to 1024 characters) with a constant of 252+ digits or a name of 255+ characters |
| **5.3.0** | `getNumber()` in [`src/legacy/engine/mathexpr.c:200`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/mathexpr.c#L200) (buffer at [`:191`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/mathexpr.c#L191)) and `getToken()` at [`:157`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/mathexpr.c#L157) (buffer at [`:76`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/mathexpr.c#L76)) |
| **5.2.4** | Same code, [`src/solver/mathexpr.c:200`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/mathexpr.c#L200) and [`:157`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/mathexpr.c#L157) |
| **6.0.0** | Not affected. Its tokenizers take numbers and names as `std::string` substrings ([`src/engine/math/MathExpr.cpp:105`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/math/MathExpr.cpp#L105), [`src/engine/quality/Treatment.cpp:140`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/Treatment.cpp#L140)). |
| **Since** | The oldest code on GitHub (2014 initial commit), so every 5.x release |
| **Fix** | Stop at the end of each buffer and flag a parse error: [`CRASH-05_swmm530.patch`](CRASH-05_swmm530.patch) |

## The problem

The expression lexer copies each number and each name it reads into a fixed buffer of 255 characters, one character at a time, without checking the length. Expressions come from input lines of up to 1024 characters (`MAXLINE`), assembled by `treatmnt_readExpression()`, `gwater_readFlowExpression()` and `controls_addExpression()`, so a single constant or name can be four times longer than the buffer.

The test has two decks:

- `[TREATMENT]` `J1 TP C = 0.5*111...1` with a 300-digit constant: `getNumber()` writes past its local `sNumber[255]` on the stack.
- `[CONTROLS]` `EXPRESSION E1 = AAA...A + 1` with a 300-character name: `getToken()` writes past the global `Token[255]` into the variables that follow it (AddressSanitizer names `Ivar`, 33 bytes further on).

Without a sanitizer what happens depends on the compiler and the memory layout: the stack write runs over the saved state of `getNumber()`, the global write over the lexer's other state. AddressSanitizer stops both at the first byte past the end.

## Why it happens

```c
// src/legacy/engine/mathexpr.c
static char   Token[255];                  // line 76

void getToken()
{
    char c[] = " ";
    Token[0] = '\0';
    while ( Pos <= Len &&
        ( isLetter(S[Pos]) || isDigit(S[Pos]) ) )
    {
        c[0] = S[Pos];
        strcat(Token, c);                  // no bound
        Pos++;
    }
    Pos--;
}

double getNumber()
{
    char c[] = " ";
    char sNumber[255];
    ...
    while (Pos < Len && isDigit(S[Pos]))
    {
        c[0] = S[Pos];
        strcat(sNumber, c);                // no bound; the same in the fraction
        Pos++;                             // and exponent loops
    }
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-05_long-number.inp`](CRASH-05_long-number.inp) | Junction J1 with `TP C = 0.5*` followed by 300 digits in `[TREATMENT]` |
| [`CRASH-05_long-name.inp`](CRASH-05_long-name.inp) | The same network with `EXPRESSION E1 = ` followed by a 300-character name and `+ 1` in `[CONTROLS]` |
| [`CRASH-05_test.c`](CRASH-05_test.c) | Opens each deck with `swmm_open()` in a child process (5.2.4, 5.3.0), so the report for the first deck does not hide the second |
| [`CRASH-05_test6.c`](CRASH-05_test6.c) | Opens and initializes both decks through the 6.0.0 API |
| [`CRASH-05_swmm530.patch`](CRASH-05_swmm530.patch) | The fix |

```sh
tools/run-test.sh CRASH-05            # 5.2.4 and 5.3.0: CRASH; 6.0.0: PASS
tools/run-test.sh CRASH-05 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix**, 5.3.0 (paths shortened; 5.2.4 shows the same frames, with `treatmnt.c:150` and `controls.c:383` as the callers):

```
==12952==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7f9acf800b2f at pc 0x55d7879370ce bp 0x7fffa705bb50 sp 0x7fffa705b2f0
    #0 0x55d7879370cd in strcat (test.bin+0xae0cd)
    #1 0x7f9ad1949129 in getNumber mathexpr.c:200:9
    #2 0x7f9ad194824e in getLex mathexpr.c:303:22
    #3 0x7f9ad1947a64 in getOp mathexpr.c:419:16
    #4 0x7f9ad19473a4 in getTree mathexpr.c:450:12
    #5 0x7f9ad194720f in mathexpr_create mathexpr.c:756:12
    #6 0x7f9ad19e199a in treatmnt_readExpression treatmnt.c:141:16
    [48, 303) 'sNumber' (line 191) <== Memory access at offset 303 overflows this variable
CRASH-05_long-number.inp     <-- memory error while reading the expression
==12966==ERROR: AddressSanitizer: global-buffer-overflow on address 0x7f9ad1aa199f at pc 0x55d7879370ce bp 0x7fffa705bc30 sp 0x7fffa705b3d0
    #0 0x55d7879370cd in strcat (test.bin+0xae0cd)
    #1 0x7f9ad19482fb in getToken mathexpr.c:157:9
    #2 0x7f9ad19482fb in getLex mathexpr.c:296:13
    #3 0x7f9ad19479c0 in getOp mathexpr.c:405:12
    #4 0x7f9ad19473a4 in getTree mathexpr.c:450:12
    #5 0x7f9ad194720f in mathexpr_create mathexpr.c:756:12
    #6 0x7f9ad18bb528 in controls_addExpression controls.c:910:12
0x7f9ad1aa199f is located 0 bytes after global variable 'Token' defined in 'mathexpr.c:76' (0x7f9ad1aa18a0) of size 255
CRASH-05_long-name.inp       <-- memory error while reading the expression
FAIL: 2 of 2 decks write past a 255-byte buffer while reading an expression
CRASH-05 5.3.0 base: CRASH
```

6.0.0 reads both decks without a memory error:

```
Deck                        open/init  Message
CRASH-05_long-number.inp            0  (accepted)
CRASH-05_long-name.inp              0  (accepted)
PASS: both decks are read without a memory error
```

(It accepts the second deck because 6.0.0 evaluates an undefined name in a control expression as 0 instead of rejecting it; that is a separate 6.0.0 issue.)

**With the fix**, 5.3.0 stops at the end of each buffer and rejects the expression:

```
Deck                        swmm_open  Report
CRASH-05_long-number.inp          200  ERROR 233: invalid math expression at line 47 of [TREATMENT] section:
CRASH-05_long-name.inp            200  ERROR 233: invalid math expression at line 40 of [CONTROL] section:
PASS: both decks are read without a memory error
CRASH-05 5.3.0 patched: PASS
```

## The fix

Check the length before each append and set the parser's error flag when the buffer is full:

```diff
     while ( Pos <= Len &&
         ( isLetter(S[Pos]) || isDigit(S[Pos]) ) )
     {
+        // --- a name too long for Token is an error
+        if (strlen(Token) >= sizeof(Token) - 1) { Err = 1; break; }
         c[0] = S[Pos];
         strcat(Token, c);
```
```diff
     while (Pos < Len && isDigit(S[Pos]))
     {
+        /* --- a number too long for sNumber is an error (room is
+               kept for the '.', 'E' and sign appended below) */
+        if (strlen(sNumber) >= sizeof(sNumber) - 4) { Err = 1; break; }
         c[0] = S[Pos];
```

The fraction and exponent loops get the same check. Each digit loop stops at 251 characters, which leaves room for the single `.`, `E` and sign that `getNumber()` appends between them, so `sNumber` holds at most 254 characters and its terminator. The expression is then rejected with ERROR 233, as for any other unreadable expression. A double carries only 17 significant digits, so no meaningful constant is refused; a name of 255 or more characters is now rejected instead of corrupting memory. There is no 6.0.0 patch. **Effect on other models:** none of the 73 regression decks has a `[TREATMENT]` or `[GWF]` section or a control-rule expression.
