#############################
#                           #
# Build a Tortoise Language #
#                           #
#############################

!ifndef outdir
!ERROR The outdir variable has to be set!
!endif
!ifndef ISO
!ERROR The ISO variable has to be set!
!endif
!ifndef LANGID
!ERROR The LANGID variable has to be set!
!endif

RESTEXT		= $(outdir)ResText.exe
APPLY		= apply
LANGDLLS	=	$(outdir)TortoiseShell$(LANGID).dll \
				$(outdir)TortoiseProc$(LANGID).dll


all: $(LANGDLLS)

clean:
	del $(LANGDLLS)

$(outdir)TortoiseShell$(LANGID).dll: Tortoise_$(ISO).po $(outdir)TortoiseShellLang.dll
  $(RESTEXT) $(APPLY) $(outdir)TortoiseShellLang.dll $@ Tortoise_$(ISO).po 

$(outdir)TortoiseProc$(LANGID).dll: Tortoise_$(ISO).po $(outdir)TortoiseProcLang.dll
  $(RESTEXT) $(APPLY) $(outdir)TortoiseProcLang.dll $@ Tortoise_$(ISO).po