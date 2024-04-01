#!/bin/bash

tail log.log -n +28 | head -n 20 > trimmed.log
sed -i 's/\/home\/mini\/workspace\/dynamoChannel\/fig_example/path\/to/g' trimmed.log

#termtosvg --still-frames --screen-geometry 80x24 --command 'cat trimmed.log' .
termtosvg --still-frames --screen-geometry 86x21 --command 'cat trimmed.log' .
