/* strdup() replacement (from stdwin, if you must know) */

#ifndef INCLUDE_HOOK_FOPEN_h__
#define INCLUDE_HOOK_FOPEN_h__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE* hook_fopen(const char* _FileName, const char* _Mode);
int hook_open(const char* _FileName, int _OFlag, int _PMode);

#ifdef MS_WINDOWS
FILE* hook_wfopen(const wchar_t* _FileName, const wchar_t* _Mode);
int hook_wopen(const wchar_t* _FileName, int _OFlag, int _PMode);
#endif

size_t fgetlinelen(FILE* file, size_t max_char_size);


#ifdef __cplusplus
}
#endif

#endif
