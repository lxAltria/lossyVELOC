#!/bin/bash -x

# SCR cleanup script
# Usage:
# ./cleanup.sh [file]
# where file contains a list of items to remove

if [ $# -gt 1 ]; then
	echo "Usage: $0 [output]"
	exit 1
fi

rm -rf /tmp/$USER/scr.*/
rm -rf .scr/
rm -f marker.veloc
if [ $# -eq 1 ]; then
	rm -rf $1
fi

exit 0