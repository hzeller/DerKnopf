# -*- Makefile -*-

%-fab.zip : %-fab.kicad_pcb
	python ../kicad-scripts/kicad-fab.py $<
	zip -r $@ fab/

%-fab.kicad_pcb : %.kicad_pcb
	sed "s/%%gitversion%%/`git log --date=short --pretty=format:'%h@%cd' -n 1 $^`/" < $^ > $@
