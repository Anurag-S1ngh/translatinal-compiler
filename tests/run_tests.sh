#!/usr/bin/env bash
set -e

TRCO="./trco"

if [ ! -f "$TRCO" ]; then
    echo "Error: trco binary not found. Run 'make' first."
    exit 1
fi

echo "Running trco test suite..."

failed=0

# Test 1: fib.c (should succeed across lexer, parser, sema, IR)
echo -n "Test fib.c [--tokens]: "
if "$TRCO" tests/cases/fib.c --tokens > /dev/null; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi

echo -n "Test fib.c [--ast]: "
if "$TRCO" tests/cases/fib.c --ast > /dev/null; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi

echo -n "Test fib.c [--ir]: "
if "$TRCO" tests/cases/fib.c --ir > /dev/null; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi

# Test 2: loops.c [--ir]
echo -n "Test loops.c [--ir]: "
if "$TRCO" tests/cases/loops.c --ir > /dev/null; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi

# Test 3: undeclared.c (should fail semantic analysis with exit code 1)
echo -n "Test undeclared.c [should fail sema]: "
if "$TRCO" tests/cases/undeclared.c --ir > /dev/null 2>&1; then
    echo "FAILED (expected failure)"
    failed=$((failed + 1))
else
    echo "PASSED"
fi

# Test 4: translation emits recompilable C (fib.c)
echo -n "Test fib.c [--translate recompiles]: "
TMPDIR_T=$(mktemp -d)
if "$TRCO" tests/cases/fib.c --translate -o "$TMPDIR_T/fib.tr.c" \
    && gcc -std=c11 -Wall "$TMPDIR_T/fib.tr.c" -o "$TMPDIR_T/fib.tr" 2>/dev/null \
    && "$TMPDIR_T/fib.tr" | grep -q "Fib(10) = 55"; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi
rm -rf "$TMPDIR_T"

# Test 5: translation emits recompilable C (loops.c)
echo -n "Test loops.c [--translate recompiles]: "
TMPDIR_T=$(mktemp -d)
if "$TRCO" tests/cases/loops.c --translate -o "$TMPDIR_T/loops.tr.c" \
    && gcc -std=c11 -Wall "$TMPDIR_T/loops.tr.c" -o "$TMPDIR_T/loops.tr" 2>/dev/null \
    && "$TMPDIR_T/loops.tr" | grep -q "sum = 23, product = 120"; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi
rm -rf "$TMPDIR_T"

# Test 6: translation preserves semantics (differential: orig vs translated)
echo -n "Test translation correctness [differential fib+loops]: "
TMPDIR_T=$(mktemp -d)
ok=1
for case in fib loops floats nested; do
    "$TRCO" "tests/cases/$case.c" --translate -o "$TMPDIR_T/$case.tr.c" || ok=0
    gcc -std=c11 -Wall "$TMPDIR_T/$case.tr.c" -o "$TMPDIR_T/$case.tr" 2>/dev/null || ok=0
    gcc -std=c11 -Wno-implicit-function-declaration "tests/cases/$case.c" -o "$TMPDIR_T/$case.orig" 2>/dev/null || ok=0
    if ! diff <("$TMPDIR_T/$case.orig") <("$TMPDIR_T/$case.tr") > /dev/null; then
        ok=0
    fi
done
if [ "$ok" -eq 1 ]; then
    echo "PASSED"
else
    echo "FAILED (translated output diverged)"
    failed=$((failed + 1))
fi
rm -rf "$TMPDIR_T"

# Test 7: undeclared.c must also fail translation
echo -n "Test undeclared.c [--translate should fail]: "
if "$TRCO" tests/cases/undeclared.c --translate > /dev/null 2>&1; then
    echo "FAILED (expected failure)"
    failed=$((failed + 1))
else
    echo "PASSED"
fi

# Test 8: stdin input ('-') produces the same translation as a file
echo -n "Test stdin input [- reads source from stdin]: "
TMPDIR_T=$(mktemp -d)
if "$TRCO" - --translate < tests/cases/fib.c > "$TMPDIR_T/stdin.c" \
    && "$TRCO" tests/cases/fib.c --translate > "$TMPDIR_T/file.c" \
    && diff "$TMPDIR_T/stdin.c" "$TMPDIR_T/file.c" > /dev/null; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi
rm -rf "$TMPDIR_T"

# Test 9: -o writes a non-empty, re-compilable target file
echo -n "Test -o writes re-compilable target file: "
TMPDIR_T=$(mktemp -d)
if "$TRCO" tests/cases/loops.c --translate -o "$TMPDIR_T/loops.target.c" \
    && [ -s "$TMPDIR_T/loops.target.c" ] \
    && gcc -std=c11 -Wall "$TMPDIR_T/loops.target.c" -o "$TMPDIR_T/loops.target" \
    && "$TMPDIR_T/loops.target" | grep -q "sum = 23, product = 120"; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi
rm -rf "$TMPDIR_T"

# Test 10: --emit-c and --c aliases produce the same output as --translate
echo -n "Test --emit-c/--c aliases: "
TMPDIR_T=$(mktemp -d)
if "$TRCO" tests/cases/fib.c --emit-c > "$TMPDIR_T/emit.c" \
    && "$TRCO" tests/cases/fib.c --c > "$TMPDIR_T/c.c" \
    && "$TRCO" tests/cases/fib.c --translate > "$TMPDIR_T/translate.c" \
    && diff "$TMPDIR_T/emit.c" "$TMPDIR_T/translate.c" > /dev/null \
    && diff "$TMPDIR_T/c.c" "$TMPDIR_T/translate.c" > /dev/null; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi
rm -rf "$TMPDIR_T"

# Test 11: the simple form — --input a.c --output o.c translates and recompiles
echo -n "Test simple form [--input a.c --output o.c]: "
TMPDIR_T=$(mktemp -d)
if "$TRCO" --input tests/cases/loops.c --output "$TMPDIR_T/out.c" \
    && [ -s "$TMPDIR_T/out.c" ] \
    && gcc -std=c11 -Wall "$TMPDIR_T/out.c" -o "$TMPDIR_T/out" \
    && "$TMPDIR_T/out" | grep -q "sum = 23, product = 120"; then
    echo "PASSED"
else
    echo "FAILED"
    failed=$((failed + 1))
fi
rm -rf "$TMPDIR_T"

# Test 12: conflicting input specifications are rejected
echo -n "Test input conflict [--input + positional] fails: "
if "$TRCO" --input tests/cases/fib.c tests/cases/loops.c --translate > /dev/null 2>&1; then
    echo "FAILED (expected failure)"
    failed=$((failed + 1))
else
    echo "PASSED"
fi

if [ $failed -eq 0 ]; then
    echo "All tests passed successfully!"
    exit 0
else
    echo "$failed test(s) failed."
    exit 1
fi
