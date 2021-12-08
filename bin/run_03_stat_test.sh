#!/bin/bash

image-5-input() {
    cat <<EOF
mount
stat 1
stat 2
stat 3
EOF
}

image-5-output() {
    cat <<EOF
disk mounted.
inode 1 has size 965 bytes.
stat failed!
stat failed!
EOF
}

image-20-input() {
    cat <<EOF
mount
stat 1
stat 2
stat 3
EOF
}

image-20-output() {
    cat <<EOF
disk mounted.
stat failed!
inode 2 has size 27160 bytes.
inode 3 has size 9546 bytes.
EOF
}

image-200-input() {
    cat <<EOF
mount
stat 1
stat 2
stat 3
stat 9
EOF
}

image-200-output() {
    cat <<EOF
disk mounted.
inode 1 has size 1523 bytes.
inode 2 has size 105421 bytes.
stat failed!
inode 9 has size 409305 bytes.
EOF
}

test-stat() {
    BLOCKS=$1

    printf "  %-58s... " "stat on data/image.$BLOCKS"
    if diff -u <(image-$BLOCKS-input | ./bin/sfssh data/image.$BLOCKS $BLOCKS 2> /dev/null | grep -v 'disk block') <(image-$BLOCKS-output) > test.log; then
    	echo "Success"
    else
    	echo "Failure"
    	cat test.log
	EXIT=$(($EXIT + 1))
    fi
    rm -f test.log
}

EXIT=0

echo
echo "Testing stat ..."
test-stat 5
test-stat 20
test-stat 200

exit $EXIT
