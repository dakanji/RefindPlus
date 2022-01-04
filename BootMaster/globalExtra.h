#define REFIT_DEBUG (0)

#define LOG_BLOCK_SEP        0
#define LOG_BLANK_LINE_SEP   1
#define LOG_LINE_SPECIAL     2
#define LOG_LINE_SAME        3
#define LOG_LINE_NORMAL      4
#define LOG_LINE_SEPARATOR   5
#define LOG_LINE_THIN_SEP    6
#define LOG_STAR_SEPARATOR   7
#define LOG_LINE_DASH_SEP    8
#define LOG_THREE_STAR_SEP   9
#define LOG_THREE_STAR_MID   10
#define LOG_THREE_STAR_END   11
#define LOG_STAR_HEAD_SEP    12
#define LOG_LINE_FORENSIC    13

VOID DebugLog (
    IN        INTN  DebugMode,
    IN  const CHAR8 *FormatString, ...
);

VOID DeepLoggger (
    IN  INTN     DebugMode,
    IN  INTN     level,
    IN  INTN     type,
    IN  CHAR16 **Msg
);

// NB: 'gLogTemp' is handled in 'DeepLoggger'
extern CHAR16 *gLogTemp;

#if REFIT_DEBUG > 0
#   define ALT_LOG(level, type, ...)                                          \
        do {                                                                  \
            gLogTemp = PoolPrint (__VA_ARGS__);                               \
            DeepLoggger (REFIT_DEBUG, level, type, &gLogTemp);                \
        } while (0)
#   define LOG_MSG(...) DebugLog (REFIT_DEBUG, __VA_ARGS__)
#else
#   define LOG_MSG(...)
#   define ALT_LOG(...)
#endif

#if REFIT_DEBUG < 1
#   define BREAD_CRUMB(...)
#   define BRK_MIN(...)
#   define BRK_MOD(...)
#   define BRK_MAX(...)
#   define LOG_SEP(...)
#elif REFIT_DEBUG < 2
#   define LOG_SEP(...)
#   define BRK_MAX(...)
#   define BRK_MOD(...) DebugLog (REFIT_DEBUG, __VA_ARGS__)
#   define BRK_MIN(...)                                                       \
        do {                                                                  \
            if (GlobalConfig.LogLevel == MINLOGLEVEL) {                       \
                DebugLog (REFIT_DEBUG, __VA_ARGS__);                          \
            }                                                                 \
        } while (0)
#   define BREAD_CRUMB(...)
#else
#   define BREAD_CRUMB(...)                                                   \
        do {                                                                  \
            if (GlobalConfig.LogLevel > MAXLOGLEVEL) {                        \
                gLogTemp = PoolPrint (__VA_ARGS__);                           \
                DeepLoggger (REFIT_DEBUG, 2, LOG_LINE_FORENSIC, &gLogTemp);   \
            }                                                                 \
        } while (0)
#   define BRK_MIN(...)                                                       \
        do {                                                                  \
            if (GlobalConfig.LogLevel == MINLOGLEVEL) {                       \
                DebugLog (REFIT_DEBUG, __VA_ARGS__);                          \
            }                                                                 \
        } while (0)
#   define BRK_MOD(...)                                                       \
        do {                                                                  \
            if (GlobalConfig.LogLevel <= MAXLOGLEVEL) {                       \
                DebugLog (REFIT_DEBUG, __VA_ARGS__);                          \
            }                                                                 \
        } while (0)
#   define BRK_MAX(...)                                                       \
        do {                                                                  \
            if (GlobalConfig.LogLevel > MAXLOGLEVEL) {                        \
                DebugLog (REFIT_DEBUG, __VA_ARGS__);                          \
            }                                                                 \
        } while (0)
#   define LOG_SEP(...)                                                       \
        do {                                                                  \
            if (GlobalConfig.LogLevel > MAXLOGLEVEL) {                        \
                gLogTemp = StrDuplicate (__VA_ARGS__);                        \
                DeepLoggger (REFIT_DEBUG, 2, LOG_BLOCK_SEP, &gLogTemp);       \
            }                                                                 \
        } while (0)
#endif
