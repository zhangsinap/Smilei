VERSION:="v$(shell git rev-list HEAD --count):$(shell git log -1 --pretty=format:%h)"

.PHONY: doc src

default: release

release:
	make -C src release

debug:
	make -C src debug

clean:
	make -C src clean
	rm -rf doc/latex doc/html
	rm -rf build
	
doc: htmldoc pdfdoc

htmldoc:
	cd doc ; (cat smilei.dox; echo "PROJECT_NUMBER=${VERSION}") | doxygen -

pdfdoc:
	cd doc/latex; pdflatex refman.tex; bibtex refman; pdflatex refman.tex
