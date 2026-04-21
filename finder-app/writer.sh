#!/bin/sh

# 1. Check if both arguments are specified
if [ "$#" -ne 2 ]; then
    echo "Error: Two arguments required."
    echo "Usage: $0 <writefile> <writestr>"
    exit 1
fi

WRITEFILE=$1
WRITESTR=$2

# 2. Extract the directory path from the full file path
# Example: /tmp/aesd/assignment.txt -> /tmp/aesd
DIRPATH=$(dirname "$WRITEFILE")

# 3. Create the directory path if it doesn't exist
# -p creates parent directories as needed and doesn't complain if they exist
if [ ! -d "$DIRPATH" ]; then
    mkdir -p "$DIRPATH"
    if [ $? -ne 0 ]; then
        echo "Error: Could not create directory $DIRPATH"
        exit 1
    fi
fi

# 4. Write the string to the file, overwriting any existing content
# We use echo directly into the file. If this fails, we exit 1.
echo "$WRITESTR" > "$WRITEFILE"

if [ $? -ne 0 ]; then
    echo "Error: Could not create or write to file $WRITEFILE"
    exit 1
fi