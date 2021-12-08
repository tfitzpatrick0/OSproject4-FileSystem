#!/bin/bash

UNIT=unit_fs
WORKSPACE=/tmp/$UNIT.$(id -u)
FAILURES=0

error() {
    echo "$@"
    [ -r $WORKSPACE/test ] && (echo; cat $WORKSPACE/test; echo)
    FAILURES=$((FAILURES + 1))
}

cleanup() {
    STATUS=${1:-$FAILURES}
    rm -fr $WORKSPACE
    exit $STATUS
}

mkdir $WORKSPACE

trap "cleanup" EXIT
trap "cleanup 1" INT TERM

echo
echo "Testing $UNIT ..."

if [ ! -x bin/$UNIT ]; then
    echo "Failure: bin/$UNIT is not executable!"
    exit 1
fi

TESTS=$(bin/$UNIT 2>&1 | tail -n 1 | awk '{print $1}')
for t in $(seq 0 $TESTS); do
    desc=$(bin/$UNIT 2>&1 | awk "/$t\./ { \$1=\$2=\"\"; print \$0 }")

    printf "%-60s... " "$desc"
    valgrind --leak-check=full bin/$UNIT $t &> $WORKSPACE/test
    if [ $? -ne 0 ] || [ $(awk '/ERROR SUMMARY:/ {print $4}' $WORKSPACE/test) -ne 0 ]; then
	error "Failure"
    else
	echo "Success"
    fi
done
