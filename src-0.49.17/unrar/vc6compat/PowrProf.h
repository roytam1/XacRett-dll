/* VC6 compat shim: extended by need. */
#ifndef _VC6_POWRPROF_H
#define _VC6_POWRPROF_H
#endif

/* VC6: decl only; Shutdown() is tool-only, never linked into our lib. */
#ifndef _VC6_SETSUSPENDSTATE_
#define _VC6_SETSUSPENDSTATE_
BOOLEAN WINAPI SetSuspendState(BOOLEAN Hibernate,BOOLEAN ForceCritical,BOOLEAN DisableWakeEvent);
#endif

/* VC6: Shutdown() is tool-only; stub so system.obj links (VC6_STUB). */
static BOOLEAN WINAPI SetSuspendState(BOOLEAN Hibernate,BOOLEAN ForceCritical,BOOLEAN DisableWakeEvent)
{ (void)Hibernate; (void)ForceCritical; (void)DisableWakeEvent; return FALSE; }
