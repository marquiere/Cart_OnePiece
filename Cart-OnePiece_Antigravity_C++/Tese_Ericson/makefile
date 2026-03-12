LATEX = latexmk
LATEX_FLAGS = -shell-escape -e "\$$pdflatex=q/pdflatex -synctex=1 -interaction=nonstopmode/" -pdf

all: main.tex
	pdflatex -interaction=nonstopmode main.tex ; \
	bibtex main; \
	pdflatex -interaction=nonstopmode main.tex ;\
	pdflatex -interaction=nonstopmode main.tex ;\
	evince main.pdf &

clean:
	rm -f             \
		*.aux         \
		*.bbl         \
		*.bcf         \
		*.blg         \
		*.fls         \
		*.log         \
		*.out         \
		*.xml         \
		*.toc         \
		*.fdb_latexmk \
		*.gz          \
		*.glo         \
		*.ist         \
		*.acn
