rem add parameter -s <emulator name> if more than one emulator
echo BEFORE START RUN PROGRAM IN EMULATOR
adb %1 %2 push ./libs/armeabi-v7a/un10c /data/local/tmp
adb %1 %2 shell chmod 777 /data/local/tmp/un10c
adb %1 %2 shell /data/local/tmp/un10c com.illuminate.texaspoker
adb %1 %2 pull /data/local/tmp/com.illuminate.texaspoker_1.dex
adb %1 %2 pull /data/local/tmp/com.illuminate.texaspoker_2.dex
adb %1 %2 pull /data/local/tmp/com.illuminate.texaspoker_3.dex
adb %1 %2 pull /data/local/tmp/com.illuminate.texaspoker_4.dex

