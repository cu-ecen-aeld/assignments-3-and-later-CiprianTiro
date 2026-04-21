#!/bin/sh

# Check if there are less than 2 arguments
if [ "$#" -ne 2 ];then
    # Check if first argument exist
    if [ -z "$1" ];then
        echo "Error: Missing arguments."
        echo "Error: The first argument (filesdir) was not specified."
        exit 1
    # Check if second argument exists
    elif [ -z "$2" ];then
        echo "Error: Missing arguments."
        echo "Error: The second argument (searchstr) was not specified."
        exit 1
    else
        echo "Error: Correct usage: $0 <filesdir> <searchstr>"
        exit 1
    fi
# Check if first argument is a path
elif [ ! -d "$1" ];then
    echo "Error: '$1' is not a valid directory on the filesystem."
    exit 1
fi

FILESDIR=$1
SEARCHSTR=$2

X=$(find "$FILESDIR" -type f -follow | wc -l)
# Calculate Y: total number of matching lines
Y=$(grep -R "$SEARCHSTR" "$FILESDIR" | wc -l)
echo "The number of files are $X and the number of matching lines are $Y"