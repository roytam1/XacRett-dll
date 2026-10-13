VC6 compatibility shims for building UnRAR 7.x with Visual C++ 6.0.
Used via /I by the UnRAR.lib build (see buildunrar7 flow + AGENTS.md).
stdint.h: VC6 predates C99 (rartypes.hpp needs it).
PowrProf.h/psapi.h/Sddl.h/Wbemidl.h/intrin.h: declare only what UnRAR 7.x
actually calls; missing VC98 SDK libs were de-pragmaed in os.hpp and the
corresponding features degrade gracefully (see patchunrar7*.py).
