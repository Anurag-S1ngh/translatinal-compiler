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
for case in fib loops; do
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

if [ $failed -eq 0 ]; then
    echo "All tests passed successfully!"
    exit 0
else
    echo "$failed test(s) failed."
    exit 1
fi
