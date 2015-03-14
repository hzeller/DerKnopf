bottom.dxf: bottom.fig
bottom.svg: bottom.fig

%.eps: %.fig
	fig2dev -L eps $< $@

%.svg: %.fig
	fig2dev -L svg $< $@

%.dxf: %.eps
	pstoedit -psarg "-r600x600" -dt -f dxf:-polyaslines $< $@

