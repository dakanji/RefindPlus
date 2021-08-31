#define REFIT_DEBUG (0)

#define LOG_BLANK_LINE_SEP   0
#define LOG_LINE_SPECIAL     1
#define LOG_LINE_NORMAL      2
#define LOG_LINE_SEPARATOR   3
#define LOG_LINE_THIN_SEP    4
#define LOG_STAR_SEPARATOR   5
#define LOG_LINE_DASH_SEP    6
#define LOG_THREE_STAR_SEP   7
#define LOG_THREE_STAR_MID   8
#define LOG_THREE_STAR_END   9
#define LOG_STAR_HEAD_SEP    10

VOID
DebugLog (
    IN        INTN  DebugMode,
    IN  const CHAR8 *FormatString, ...
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
  IN  CHAR16 **Msg
);

// NB: 'gLogTemp' is freed in DeepLoggger
#define LOG(level, type, ...) \
        gLogTemp = PoolPrint(__VA_ARGS__); \
        DeepLoggger(REFIT_DEBUG, level, type, &gLogTemp);
#endif
