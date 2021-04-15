#define REFIT_DEBUG (0)

#define LOG_STAR_HEAD_SEP  0
#define LOG_LINE_NORMAL      1
#define LOG_LINE_SEPARATOR   2
#define LOG_LINE_THIN_SEP    3
#define LOG_STAR_SEPARATOR   4
#define LOG_LINE_DASH_SEP    5
#define LOG_THREE_STAR_SEP   6
#define LOG_THREE_STAR_MID   7
#define LOG_THREE_STAR_END   8

VOID
DebugLog (
  IN        INTN  DebugMode,
  IN  CONST CHAR8 *FormatString, ...
);

#if REFIT_DEBUG == 0
  #define MsgLog(...)
#else
  #define MsgLog(...)  DebugLog(REFIT_DEBUG, __VA_ARGS__)
#endif

#if REFIT_DEBUG > 0
extern CHAR16 *gLogTemp;

VOID
DeepLoggger (
  IN  INTN     DebugMode,
  IN  INTN     level,
  IN  INTN     type,
  IN  CHAR16 **Message
);

// NB: gLogTemp is freed in DeepLoggger
#define LOG(level, type, ...) \
        gLogTemp = PoolPrint(__VA_ARGS__); \
        DeepLoggger(REFIT_DEBUG, level, type, &gLogTemp);
#endif
