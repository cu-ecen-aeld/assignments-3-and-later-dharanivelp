#!/bin/sh

writefile=$1
writestr=$2

if [ -z $1 ] || [ -z $2 ]
then
	echo "ERROR: missing arguments"
	echo "expected: writer.sh [writefile] [writestr]"
	exit 1
fi

dirpath=$(dirname "$1")

if [ ! -d $dirpath ]
then
	echo "directory $dirpath doesn't exist"
	mkdir -p $dirpath
	if [ $? -eq 0 ]
	then
		echo "created now"
	else
		echo "cannot create directory"
		exit 1
	fi
fi

if [ -d $dirpath ]
then
	touch $writefile
	echo $2 > $writefile
	echo "string $2 written to $1 success"
	if [ $? != 0 ]
	then
		echo "ERROR: unable to write"
		exit 1
	fi
fi
