#!/bin/sh

make chiton
cp bin/chiton release/chiton
cp bin/update_json release/update_json
#diff release/libaditof.so.1.5.0 /home/root/aditof_sdk/build6/sdk/libaditof.so.1.5.0
