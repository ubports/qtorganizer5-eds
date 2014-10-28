#!/bin/sh

#ARG1 - DBUS RUNNERN PATH
#ARG2 - TEST EXECUTABLE FULL PATH
#ARG3 - TEST NAME
#ARG4 - EVOLUTION_CALENDAR_FACTORY FULL PATH
#ARG5 - EVOLUTION_CALENDAR_FACTORY SERVICE NAME
#ARG6 - EVOLUTION_SOURCE_REGISTRY FULL PATH


export TEST_TMP_DIR=$(mktemp -d /tmp/$3_XXXX)
export QT_QPA_PLATFORM=minimal
export HOME=$TEST_TMP_DIR
export XDG_RUNTIME_DIR=$TEST_TMP_DIR
export XDG_CACHE_HOME=$TEST_TMP_DIR}/.cache
export XDG_CONFIG_HOME=$TEST_TMP_DIR/.config
export XDG_DATA_HOME=$TEST_TMP_DIR/.local/share
export XDG_DESKTOP_DIR=$TEST_TMP_DIR
export XDG_DOCUMENTS_DIR=$TEST_TMP_DIR
export XDG_DOWNLOAD_DIR=$TEST_TMP_DIR
export XDG_MUSIC_DIR=$TEST_TMP_DIR
export XDG_PICTURES_DIR=$TEST_TMP_DIR
export XDG_PUBLICSHARE_DIR=$TEST_TMP_DIR
export XDG_TEMPLATES_DIR=$TEST_TMP_DIR
export XDG_VIDEOS_DIR=$TEST_TMP_DIR
export QORGANIZER_EDS_DEBUG=On

echo HOMEDIR=$HOME
rm -rf $XDG_DATA_HOME
$1 --keep-env --max-wait=90 \
--task $2 --task-name $3 --wait-until-complete --wait-for=org.gnome.evolution.dataserver.Calendar4 \
--task $4 --task-name "evolution" -r
#--task $6 --task-name "source-registry" --wait-for=org.gtk.vfs.Daemon -r \
#--task $7 --task-name "gvfsd" -r
return $?
