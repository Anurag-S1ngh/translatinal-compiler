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

if [ $failed -eq 0 ]; then
    echo "All tests passed successfully!"
    exit 0
else
    echo "$failed test(s) failed."
    exit 1
fi
