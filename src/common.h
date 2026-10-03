#pragma once
// Shared between the Steam networking proxy and the sync fixes.
void Log(const char *fmt, ...);
void SyncFix_Apply(const char *ini);
