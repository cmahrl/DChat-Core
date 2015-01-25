ACLOCAL_AMFLAGS = -I m4
CFLAGS = --pedantic -Wall

SUBDIRS = src man

include aminclude.am

EXTRA_DIST = $(DX_CONFIG) logo.png doc

doc_DATA = README INSTALL NEWS 

.PHONY: changelog doxygen-run doxygen-doc $(DX_PS_GOAL) $(DX_PDF_GOAL)

changelog: ; build/gitlog-to-changelog > ChangeLog;
