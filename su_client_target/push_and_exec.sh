#!/bin/bash
APP_NAME=su_client
TMP_PATH=/data/local/tmp
adb -d push libs/armeabi-v7a/$APP_NAME $TMP_PATH/$APP_NAME
adb -d shell chmod 755 $TMP_PATH/$APP_NAME
adb -d shell $TMP_PATH/$APP_NAME -c 'ls -l /'
