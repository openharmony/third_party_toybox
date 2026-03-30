/* id.c - print real and effective user and group IDs
 *
 * Copyright 2012 Sony Network Entertainment, Inc.
 *
 * by Tim Bird <tim.bird@am.sony.com>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/id.html

USE_ID(NEWTOY(id, ">1"USE_ID_Z("Z")"nGgru[!"USE_ID_Z("Z")"Ggu]", TOYFLAG_USR|TOYFLAG_BIN))
USE_GROUPS(NEWTOY(groups, NULL, TOYFLAG_USR|TOYFLAG_BIN))
USE_LOGNAME(NEWTOY(logname, ">0", TOYFLAG_USR|TOYFLAG_BIN))
USE_WHOAMI(OLDTOY(whoami, logname, TOYFLAG_USR|TOYFLAG_BIN))

config ID
  bool "id"
  default y
  help
    usage: id [-Ggnru] [USER...]

    Print user and group ID.

    -G	Show all group IDs
    -g	Show only the effective group ID
    -n	Print names instead of numeric IDs (to be used with -Ggu)
    -r	Show real ID instead of effective ID
    -u	Show only the effective user ID

config ID_Z
  bool
  default y
  depends on ID && !TOYBOX_LSM_NONE
  help
    usage: id [-Z]

    -Z	Show only security context

config GROUPS
  bool "groups"
  default y
  help
    usage: groups [user]

    Print the groups a user is in.

config LOGNAME
  bool "logname"
  default y
  help
    usage: logname

    Print the current user name.

config WHOAMI
  bool "whoami"
  default y
  help
    usage: whoami

    Print the current user name.
*/

#define FOR_id
#define FORCE_FLAGS
#include "toys.h"

GLOBALS(
  int is_groups;
)

#ifdef TOYBOX_OH_ADAPT
void* dlopen_handle = NULL;
typedef struct passwd* (*oh_getpwuid_)(uid_t);
typedef struct passwd* (*oh_getpwnam_)(char *);
typedef struct group* (*oh_getgrgid_)(gid_t gid);
oh_getpwuid_ oh_getpwuid_func = NULL;
oh_getpwnam_ oh_getpwnam_func = NULL;
oh_getgrgid_ oh_getgrgid_func = NULL;

static void do_id_init()
{
  if (dlopen_handle) {
    dlclose(dlopen_handle);
    dlopen_handle = NULL;
  }
  dlopen_handle = dlopen("libaccount_posix_adapter.z.so", RTLD_LAZY);
  if (!dlopen_handle) return;
  oh_getpwuid_func = (oh_getpwuid_)dlsym(dlopen_handle, "oh_getpwuid");
  oh_getpwnam_func = (oh_getpwnam_)dlsym(dlopen_handle, "oh_getpwnam");
  oh_getgrgid_func = (oh_getgrgid_)dlsym(dlopen_handle, "oh_getgrgid");
}

static struct passwd *oh_getpwuid(uid_t uid)
{
  if (!oh_getpwuid_func) return NULL;
  return oh_getpwuid_func(uid);
}

static struct passwd *oh_getpwnam(char *name)
{
  if (!oh_getpwnam_func) return NULL;
  const char *copy = strdup(name);
  if (!copy) return NULL;
  struct passwd *pw = oh_getpwnam_func(copy);
  free(copy);
  return pw;
}

struct group *oh_getgrgid(gid_t gid)
{
  if (!oh_getgrgid_func) return NULL;
  return oh_getgrgid_func(gid);
}
#endif

static void showone(char *prefix, char *s, unsigned u, int done)
{
  if (FLAG(n)) printf("%s%s", prefix, s);
  else printf("%s%u", prefix, u);
  if (done) {
    xputc('\n');
#ifdef TOYBOX_OH_ADAPT
    dlclose(dlopen_handle);
    dlopen_handle = NULL;
#endif
    xexit();
  }
}

static void showid(char *prefix, unsigned u, char *s)
{
  printf("%s%u(%s)", prefix, u, s);
}


static void do_id(char *username)
{
#ifdef TOYBOX_OH_ADAPT
  do_id_init();
#endif
  struct passwd *pw;
  struct group *grp;
  uid_t uid = getuid(), euid = geteuid();
  gid_t gid = getgid(), egid = getegid();
  gid_t *groups = (gid_t *)toybuf;
  int i = sizeof(toybuf)/sizeof(gid_t), ngroups;

  // check if a username is given
  if (username) {
    pw = getpwnam(username);
#ifdef TOYBOX_OH_ADAPT
    if (!pw) pw = oh_getpwnam(username);
#endif
    if (!pw) {
      uid = atolx_range(username, 0, INT_MAX);
      if ((pw = bufgetpwuid(uid))) username = pw->pw_name;
#ifdef TOYBOX_OH_ADAPT
      if ((!pw) && (pw = oh_getpwuid(uid))) username = pw->pw_name;
#endif
    }
    if (!pw) error_exit("no such user '%s'", username);
    uid = euid = pw->pw_uid;
    gid = egid = pw->pw_gid;
    if (TT.is_groups) printf("%s : ", pw->pw_name);
  }

#ifdef TOYBOX_OH_ADAPT
  pw = oh_getpwuid(FLAG(r) ? uid : euid);
  if (!pw) pw = xgetpwuid(FLAG(r) ? uid : euid);
#else
  pw = xgetpwuid(FLAG(r) ? uid : euid);
#endif
  if (FLAG(u)) showone("", pw->pw_name, pw->pw_uid, 1);

#ifdef TOYBOX_OH_ADAPT
  grp = oh_getgrgid(FLAG(r) ? gid : egid);
  if (!grp) grp = xgetgrgid(FLAG(r) ? gid : egid);
#else
  grp = xgetgrgid(FLAG(r) ? gid : egid);
#endif
  if (FLAG(g)) showone("", grp->gr_name, grp->gr_gid, 1);

  ngroups = username ? getgrouplist(username, gid, groups, &i)
    : getgroups(i, groups);
  if (ngroups<0) perror_exit("getgroups");

  if (FLAG(G)) {
    showone("", grp->gr_name, grp->gr_gid, 0);
    for (i = 0; i<ngroups; i++) {
      if (groups[i] != egid) {
#ifdef TOXBOX_OH_ADAPT
        if ((grp=getgrgid(groups[i])) || (grp=oh_getgrgid(groups[i]))) showone(" ",grp->gr_name,grp->gr_gid,0);
#else
        if ((grp=getgrgid(groups[i]))) showone(" ",grp->gr_name,grp->gr_gid,0);
#endif
        else printf(" %u", groups[i]);
      }
    }
    xputc('\n');
    return;
  }

  if (!FLAG(Z)) {
    showid("uid=", pw->pw_uid, pw->pw_name);
    showid(" gid=", grp->gr_gid, grp->gr_name);

    if (!FLAG(r)) {
      if (uid != euid) {
#ifdef TOXBOX_OH_ADAPT
        pw = oh_getpwuid(euid);
        if (!pw) pw = xgetpwuid(euid);
#else
        pw = xgetpwuid(euid);
#endif
        showid(" euid=", pw->pw_uid, pw->pw_name);
      }
      if (gid != egid) {
#ifdef TOXBOX_OH_ADAPT
        grp = oh_getgrgid(egid);
        if (!grp) grp = xgetgrgid(egid);
#else
        grp = xgetgrgid(egid);
#endif
        showid(" egid=", grp->gr_gid, grp->gr_name);
      }
    }

    showid(" groups=", gid, grp->gr_name);
    for (i = 0; i<ngroups; i++) {
      if (groups[i] != egid) {
#ifdef TOXBOX_OH_ADAPT
        if ((grp=getgrgid(groups[i])) || (grp=oh_getgrgid(groups[i]))) showid(",", grp->gr_gid, grp->gr_name);
#else
        if ((grp=getgrgid(groups[i]))) showid(",", grp->gr_gid, grp->gr_name);
#endif
        else printf(",%u", groups[i]);
      }
    }
  }

  if (!CFG_TOYBOX_LSM_NONE) {
    if (lsm_enabled()) {
      char *context = lsm_context();

      printf("%s%s", FLAG(Z) ? "" : " context=", context);
      if (CFG_TOYBOX_FREE) free(context);
    } else if (FLAG(Z)) error_exit("%s disabled", lsm_name());
  }

  xputc('\n');
#ifdef TOYBOX_OH_ADAPT
  dlclose(dlopen_handle);
  dlopen_handle = NULL;
#endif
}

void id_main(void)
{
  if (toys.optc) while(*toys.optargs) do_id(*toys.optargs++);
  else do_id(NULL);
}

void groups_main(void)
{
  TT.is_groups = 1;
  toys.optflags = FLAG_G|FLAG_n;
  id_main();
}

void logname_main(void)
{
  toys.optflags = FLAG_u|FLAG_n;
  id_main();
}
