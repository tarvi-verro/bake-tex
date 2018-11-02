# Execute in the directory where the %.tex is at, using -f../Makefile

ifeq ($(.DEFAULT_GOAL),)
nothing:
	@echo "Type in the PDF you'd like me to muster."
endif

B:=$(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

# List of source files
ifneq ($(MAKECMDGOALS),)
SRC:=$(MAKECMDGOALS)
else
SRC:=$(.DEFAULT_GOAL)
endif

#echo "compiling $@...";
SVGLAYER_TO_PDF = \
	name=`echo "$(notdir $@)" | sed "s@--.*@@"`; \
	layer=`echo "$@" | sed "s@.*--\([^.]*\)\.pdf_tex@\1@"`; \
	echo "compiling $$name--$$layer.pdf_tex..."; \
	xmlstarlet ed \
		-d "/_:svg/_:g[@inkscape:groupmode='layer']/@style" \
		-i "/_:svg/_:g[@inkscape:groupmode='layer']" -t attr -n "style" \
			-v "display:none" \
		-u "/_:svg/_:g[@inkscape:label='$$layer']/@style" -v "display:inline" \
		$$name.svg \
	| inkscape --file=/dev/stdin --export-area-page \
		--export-pdf=.aux/$${name}--$${layer}.pdf --export-latex 2>&1 \
	| grep -v "^$$\|Format autodetect failed. The file is being opened as SVG.";\
	[ $$? -ne 0 ] # negate rval: grep only succeeds when lines come through

SVGLAYER_SUM = \
	name=`echo "$(notdir $@)" | sed "s@--.*@@"`; \
	layer=`echo "$@" | sed "s@.*--\([^.]*\)\.sum@\1@"`; \
	sum=`xmlstarlet sel -t -c "/_:svg/_:g[@inkscape:label='$$layer']/*" $$name.svg | cksum`; \
	echo "$$sum" | cmp -s - $@; \
	if [ $$? -ne 0 ]; then \
		echo "$$sum" > .aux/$(notdir $@); \
	fi

.aux::
	@mkdir -p .aux

%.pdf: %.tex .aux/%.dep | .aux
	@ln -sf ../$*.pdf .aux/$*.pdf
	@ln -sf ../$*.tex .aux/$*.tex
	@rm -f $@
	@cd .aux && rubber --module xelatex $*.tex

# Default, all layers render
.aux/%.pdf_tex: %.svg | .aux
	@echo "compiling $*.pdf_tex..."
	@mkdir -p $(dir $@)
	@inkscape --file=$*.svg --export-pdf=.aux/$*.pdf --export-latex

# Gnuplot rendering.
# To add new commands to the "set terminal cairolatex pdf color" line, use the
# following syntax on the very first line of the script:
#
#	# term-cmds: <commands>
#
# These will be appended to the "set terminal" command line.
.aux/%.gp_tex: %.gp | .aux
	@echo "compiling $*.gp_tex..."
	@mkdir -p $(dir $@)
	@cd .aux; \
		cmds=$$(head -n1 ../$*.gp | grep -oP "(?<=^# term-cmds:).*"); \
		printf "set terminal cairolatex pdf color $$cmds\nset output '$*.tex'\n" \
		| cat - ../$*.gp | gnuplot && mv "$*.tex" "$*.gp_tex"

# .xcf files rendering
.aux/%_xcf.pdf: %.xcf | .aux
	@echo "compiling $*_xcf.pdf..."
	@mkdir -p $(dir $@)
	@echo "(define (convert-xcf-to-pdf filename outfile) \
		(let* ( \
			(image (car (gimp-file-load RUN-NONINTERACTIVE filename filename))) \
			(drawable (car (gimp-image-merge-visible-layers image CLIP-TO-BOTTOM-LAYER))) \
		) \
		(file-pdf-save RUN-NONINTERACTIVE image drawable outfile outfile TRUE TRUE TRUE) \
		(gimp-image-delete image) \
		) \
	) \
	(gimp-message-set-handler 1) \
	(convert-xcf-to-pdf \"$*.xcf\" \".aux/$*_xcf.pdf\") \
	(gimp-quit 0)" | gimp -i -b - &> .aux/$*_xcf.log # gimp is whiny, so give it a log file


$B/texdeps: $B/texdeps.c
	@echo "compiling texdeps..."
	@gcc $< -Wall -o $@

.aux/%.dep: %.tex | $B/texdeps .aux
	@cat $< | $B/texdeps $*.pdf > $@


## synopsis.dep will contain:
## a) Simple, non-layered rendering
#
#	synopsis.pdf: .aux/elektrolyys.pdf_tex
#
#	synopsis.pdf: .aux/delfi-regio-juh_xcf.pdf
#
## b) Per-layer rendering, relayed over a checksum of layer data
#
#	synopsis.pdf: .aux/kov-side--H_2O.pdf_tex
#
#	.aux/kov-side--H_2O.pdf_tex: .aux/kov-side--H_2O.sum
#		@$(SVGLAYER_TO_PDF)
#
#	.aux/kov-side--H_2O.sum: kov-side.svg
#		@${SVGLAYER_SUM}
#

-include $(patsubst %.pdf,.aux/%.dep,$(filter %.pdf,$(SRC)))

%.pdf: %.ods
	@echo "compiling $*.pdf (from .ods)..."
	@mkdir -p $(dir $@)
	@OUT=`soffice --headless --convert-to pdf:writer_pdf_Export $*.ods 2>&1`; \
		SOFFICE_RV=$$?; \
		printf "$$OUT" | grep -q "error"; \
		if [ $$? -eq 0 ] || [ $$SOFFICE_RV -ne 0 ]; then \
			echo " ..failed!"; \
			echo "$$OUT"; \
		fi;

# Checks the programs required by many of these scrips
check:
	@which soffice gnuplot inkscape pdflatex rubber xmlstarlet gimp sed \
		gcc cksum grep

clean:
	@# Only deletes %.pdf's with a corresponding %.pdf_tex or %.tex
	rm -f $$(ls *.tex 2>/dev/null | sed "s@\.\(tex\)\(\$$\|\s\)@.pdf @") \
	rm -rf .aux/

.PHONY: nothing clean check
