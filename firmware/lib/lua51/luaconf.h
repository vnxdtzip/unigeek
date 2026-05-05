/*
** luaconf.h — ESP32 configuration (no PSRAM, float numbers)
** Replaces standard Lua 5.1.5 luaconf.h.
*/
#ifndef lconfig_h
#define lconfig_h

#include <limits.h>
#include <stddef.h>
#include <math.h>
#include <setjmp.h>

/* ── Integer storage types (used by llimits.h) ───────────────────────── */
#define LUAI_UINT32    unsigned int
#define LUAI_INT32     int
#define LUAI_MAXINT32  INT_MAX
#define LUAI_UMEM      size_t
#define LUAI_MEM       ptrdiff_t

/* ── Alignment (llimits.h does: typedef LUAI_USER_ALIGNMENT_T L_Umaxalign) */
#define LUAI_USER_ALIGNMENT_T  union { double u; void *s; long l; }

/* ── Number ↔ string conversion ──────────────────────────────────────── */
#define lua_number2str(s,n)   sprintf((s), LUA_NUMBER_FMT, (n))
#define lua_str2number(s,p)   strtof((s), (p))

/* ── Number: float (4 B) instead of double (8 B) ─────────────────────── */
#define LUA_NUMBER          float
#define LUA_NUMBER_SCAN     "%g"
#define LUA_NUMBER_FMT      "%.7g"
#define LUAI_UACNUMBER      float

#define lua_number2int(i,d)      ((i) = (int)(d))
#define lua_number2integer(i,n)  ((i) = (lua_Integer)(n))

#define luai_numunm(a)    (-(a))
#define luai_numeq(a,b)   ((a) == (b))
#define luai_numlt(a,b)   ((a) <  (b))
#define luai_numle(a,b)   ((a) <= (b))
#define luai_numadd(a,b)  ((a) + (b))
#define luai_numsub(a,b)  ((a) - (b))
#define luai_nummul(a,b)  ((a) * (b))
#define luai_numdiv(a,b)  ((a) / (b))
#define luai_nummod(a,b)  ((a) - floorf((a) / (b)) * (b))
#define luai_numpow(a,b)  (powf((a), (b)))
#define luai_numisnan(a)  (!luai_numeq((a), (a)))

/* ── Integer type ─────────────────────────────────────────────────────── */
#define LUA_INTEGER     ptrdiff_t
#define LUA_INTFRM_T    long
#define LUA_INTFRMLEN   "l"

/* ── Stack / call / buffer limits ────────────────────────────────────── */
#ifdef BOARD_HAS_PSRAM
  /* PSRAM boards: 8 MB PSRAM — allow VM to grow into ~2 MB */
  #define LUAI_MAXSTACK    8000   /* 8000 × 16 B = 128 KB for stack frames */
  #define LUAI_MAXCSTACK   8000
  #define LUA_MAXCAPTURES  64
  #define LUAI_MAXVARS     800
  #define LUAI_MAXUPVALUES 400
  #define LUA_BUFFERSIZE   8192
  #define LUAL_BUFFERSIZE  8192
  #define LUA_MAXINPUT     8192
  /* GC: lazy — plenty of headroom */
  #define LUAI_GCPAUSE  400
  #define LUAI_GCMUL    400
  #define LUAI_MAXCALLS   800
  #define LUAI_MAXCCALLS  800
#else
  /* No PSRAM: ~100–120 KB heap budget */
  #define LUAI_MAXSTACK    600   /* 600 × 8 B = 4.8 KB max stack */
  #define LUAI_MAXCSTACK   600
  #define LUA_MAXCAPTURES  24
  #define LUAI_MAXVARS     300
  #define LUAI_MAXUPVALUES 80
  #define LUA_BUFFERSIZE   1024
  #define LUAL_BUFFERSIZE  1024
  #define LUA_MAXINPUT     512
  /* GC: moderate — allows ~70–90 KB dynamic heap before collecting */
  #define LUAI_GCPAUSE  160
  #define LUAI_GCMUL    175
  #define LUAI_MAXCALLS   400   /* 400 × 28 B = 11 KB CallInfo */
  #define LUAI_MAXCCALLS  400
#endif

#define LUAI_BITSINT     32
#define LUAI_MAXNUMBER2STR   32

/* ── String quoting (used in lauxlib.c / lbaselib.c error messages) ──── */
#define LUA_QL(x)   "'" x "'"
#define LUA_QS      LUA_QL("%s")

/* ── User state hooks — all no-ops ───────────────────────────────────── */
#define luai_userstateopen(L)       ((void)0)
#define luai_userstateclose(L)      ((void)0)
#define luai_userstatethread(L,L1)  ((void)0)
#define luai_userstatefree(L)       ((void)0)
#define luai_userstateresume(L,n)   ((void)0)
#define luai_userstateyield(L,n)    ((void)0)

/* ── Paths (loading done via IStorage, not stdio) ────────────────────── */
#define LUA_PATH_DEFAULT   ""
#define LUA_CPATH_DEFAULT  ""
#define LUA_PATH_SEP       ";"
#define LUA_PATH_MARK      "?"
#define LUA_EXECDIR        "!"
#define LUA_IGMARK         "-"

/* ── API visibility ───────────────────────────────────────────────────── */
#define LUA_API    extern
#define LUALIB_API extern
#define LUAI_FUNC  extern
#define LUAI_DATA  extern

/* ── Error handling: setjmp/longjmp ──────────────────────────────────── */
#define LUAI_THROW(L,c)  longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)  if (setjmp((c)->b) == 0) { a }
#define l_jmpbuf         jmp_buf
#define luai_jmpbuf      jmp_buf

/* ── No extra space in lua_State ─────────────────────────────────────── */
#define LUAI_EXTRASPACE 0

/* ── Misc ─────────────────────────────────────────────────────────────── */
#define LUA_IDSIZE   60
#define LUA_PROMPT   "> "
#define LUA_PROMPT2  ">> "
#define LUA_PROGNAME "lua"

/* ── Assertions disabled for release ─────────────────────────────────── */
#define lua_assert(c)      ((void)0)
#define luai_apicheck(L,o) ((void)(L))

#endif /* lconfig_h */
