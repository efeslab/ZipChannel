DRRUN=../dynamorio/Build/bin64/drrun
TOOL=../dynamorio/Build/api/bin/libmarina.so

$DRRUN -c $TOOL -- ./code
