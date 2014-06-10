#!/bin/sh

#ARG1 - DBUS RUNNERN PATH
#ARG2 - TEST EXECUTABLE FULL PATH 
#ARG3 - TEST NAME
#ARG4 - EVOLUTION_CALENDAR_FACTORY FULL PATH
#ARG5 - EVOLUTION_CALENDAR_FACTORY SERVICE NAME
#ARG6 - EVOLUTION_SOURCE_REGISTRY FULL PATH

rm -rf $XDG_DATA_HOME
$1 --keep-env \
--task $2 --task-name $3 --wait-until-complete --wait-for=org.gnome.evolution.dataserver.Calendar4 \
--task $4 --task-name "evolution" -r --wait-for=$5 \
--task $6 --task-name "source-registry" --wait-for=org.gtk.vfs.Daemon -r \
--task $7 --task-name "gvfsd" -r
return $?
