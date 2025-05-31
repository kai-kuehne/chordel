#ifndef CHORDEL_LOG_H
#define CHORDEL_LOG_H

#define LOG_DEBUG(fmt, ...)    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)     SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)     SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, fmt, ##__VA_ARGS__)

#endif
