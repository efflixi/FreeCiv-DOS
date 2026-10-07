#ifndef FC__DOS_EXTENDER_COMPAT_H
#define FC__DOS_EXTENDER_COMPAT_H

enum dos_extender_kind {
  DOS_EXTENDER_NONE = 0,
  DOS_EXTENDER_DJGPP_DPMI = 1,
  DOS_EXTENDER_DOS4GW = 2
};

const char *dos_extender_name(void);
int dos_extender_init(void);
void dos_extender_shutdown(void);

#endif
