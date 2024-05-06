#!/bin/sh

filesdir=$1
searchstr=$2

if [ -z $1 ] || [ -z $2 ]
then
	echo "ERROR: argument missing"
	echo "expected: finder.sh [filesdir] [searchstr]"
	exit 1
fi

if [ ! -d $1 ]
then
	echo "ERROR: $1 is not a directory or doesn't exist"
	exit 1
fi

files_count=`find $1 -type f | wc -l`
str_matching_file_count=`grep -r $2 $1 | wc -l`

echo "The number of files are $files_count and the number of matching lines are $str_matching_file_count"
