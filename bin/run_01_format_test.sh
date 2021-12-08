#!/bin/bash

image-5-output() {
    cat <<EOF
disk formatted.
SuperBlock:
    magic number is valid
    5 blocks
    1 inode blocks
    128 inodes
EOF
}

image-20-output() {
    cat <<EOF
disk formatted.
SuperBlock:
    magic number is valid
    20 blocks
    2 inode blocks
    256 inodes
EOF
}

image-200-output() {
    cat <<EOF
disk formatted.
SuperBlock:
    magic number is valid
    200 blocks
    20 inode blocks
    2560 inodes
EOF
}

test-input() {
    cat <<EOF
format
debug
EOF
}

test-format() {
    DISK=$1
    BLOCKS=$2
    OUTPUT=$3

    cp $DISK $DISK.formatted
    printf "  %-58s... " "format on $DISK.formatted"
    if diff -u <(test-input | ./bin/sfssh $DISK.formatted $BLOCKS 2> /dev/null | grep -v 'disk block') <($OUTPUT) > test.log; then
    	echo "Success"
    else
    	echo "Failure"
    	cat test.log
	EXIT=$(($EXIT + 1))
    fi
    rm -f $DISK.formatted test.log
}

EXIT=0

echo
echo "Testing format ..."
test-format data/image.5   5   image-5-output
test-format data/image.20  20  image-20-output
test-format data/image.200 200 image-200-output

exit $EXIT
