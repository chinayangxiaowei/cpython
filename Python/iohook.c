#include "Python.h"
#include "pydebug.h"
#include "iohook.h"
#include "osdefs.h"
#include <wchar.h>
#include <locale.h>
#include <fcntl.h>
#include "fnmatch.h"
#include "textreader.h"


int exists(wchar_t* filename) {
#ifdef MS_WINDOWS
    return _waccess(filename, 0) == 0;
#else
    char out[MAXPATHLEN];
    if (wcstombs(out, filename, MAXPATHLEN) != -1) {
        return access(out, 0) == 0;
    }
    return 0;
#endif
}

int joinpath(wchar_t* buffer, const wchar_t* stuff) {
    size_t n, k;
    if (stuff[0] == SEP)
        n = 0;
    else {
        n = wcslen(buffer);
        if (n > 0 && buffer[n - 1] != SEP && n < MAXPATHLEN)
            buffer[n++] = SEP;
    }
    if (n > MAXPATHLEN)
        return 0;
    k = wcslen(stuff);
    if (n + k > MAXPATHLEN)
        k = MAXPATHLEN - n;
    wcsncpy(buffer + n, stuff, k);
    buffer[n + k] = '\0';
    return 1;
}


typedef struct _redirect_item {
    wchar_t pattern[MAXPATHLEN];
    wchar_t relative_path[MAXPATHLEN];
    struct _redirect_item* next;
}redirect_item;

typedef struct _redirect_list {
    unsigned int count;
    redirect_item* begin;
    redirect_item* end;
}redirect_list;


void add_redirect_item(redirect_list* list, redirect_item* item) {
    if (item) {
        item->next = NULL;

        if (list->begin == NULL)
            list->begin = item;

        if (list->end == NULL) {
            list->end = item;
        }
        else {
            list->end->next = item;
        }
        list->count++;
    }
}

unsigned int parser_redirect_list(text_line_list* list, redirect_list* redirect_list) {
    text_line_t* item;
    wchar_t* p, * e, * t;
    if (list == NULL || redirect_list==NULL) return 0;
    item = list->begin;
    list->count = 0;
    while (item) {
        p = item->text;
        while (*p == L' ' && *p != L'\0') {
            p++;
        }
        e = wcsstr(p, L"=>");
        if (e) {
            redirect_item* item = (redirect_item*)malloc(sizeof(redirect_item));
            if (item) {
                t = e;
				//reverse skip space chars
                while (*(t-1)==L' ') {
                    t--;
                }
                *t = L'\0';

                wcscpy(item->pattern, p);
                p = e + 2;
				//skip space chars
                while (*p == ' ' && *p != '\0') {
                    p++;
                }

                if (*p) {
					//skip to \r\n
                    t = p;
                    while (*t && *t!='\r'&& *t != '\n') {
                        t++;
                    }
					//reverse skip space chars
                    while (*(t - 1) == L' ') {
                        t--;
                    }
                    *t = L'\0';
                    wcscpy(item->relative_path, p);
                    add_redirect_item(redirect_list, item);
                }
                else {
                    free(item);
                }
            }
        }
        item = item->next;
    }
    return list->count;
}


unsigned int read_redirect_list(const wchar_t* path, redirect_list* list) {
    FILE* file;
    wchar_t config_fname[MAXPATHLEN];
#ifndef MS_WINDOWS
    char out[MAXPATHLEN];
#endif
    if (!list) return 0;

    list->end = list->begin = NULL;
    list->count = 0;

    wcscpy(config_fname, path);
    joinpath(config_fname, L"redirect_list.cfg");


#ifdef MS_WINDOWS
    file = _wfopen(config_fname, L"rb");
#else
    file=NULL;
    if (wcstombs(out, config_fname, MAXPATHLEN) != -1) {
    	file = fopen(out, "rb");
    }
#endif

    if (file != NULL) {
        text_line_list line_list;
        if (read_text_lines(file, &line_list)) {
            parser_redirect_list(&line_list, list);
            remove_text_line_list(&line_list);
        }
        fclose(file);
    }

    return list->count;
}


int search_hook_w(const wchar_t* filename, wchar_t* redirect_filename) {
    int ret;
    char* hook_dirs;
    wchar_t hook_dirs_w[1024*10];
    redirect_list list;
    redirect_item* item;

    wchar_t* start, * end, *t;
    const wchar_t* py_file_name, * ext_name;

    wchar_t fullfname[MAXPATHLEN];

    ext_name = wcsrchr(filename, L'.');

    if (ext_name == NULL) {
	return 0;
    }

#ifdef MS_WINDOWS
    if (wcsicmp(ext_name, L".py") != 0) {
        return 0;
    }
#else
    if (wcscasecmp(ext_name, L".py") != 0) {
        return 0;
    }
#endif



    hook_dirs = Py_GETENV("PYTHON_HOOK_DIR");
    if (hook_dirs == NULL || *hook_dirs == '\0') {
        return 0;
    }

    if (mbstowcs(hook_dirs_w, hook_dirs, 1024*10) == -1) {
        return 0;
    }

    py_file_name = wcsrchr(filename, L'\\');
    if (!py_file_name) py_file_name = wcsrchr(filename, L'/');
    if (!py_file_name) {
        py_file_name = filename;
    }
    else {
        py_file_name++;
    }

    ret = 0;
    start = hook_dirs_w;

    while (*start==L' ') {
        start++;
    }

    while (1) {
        end = wcschr(start, L';');
        if (end){
			t=end;
		}else{
			t=start+wcslen(start);
		}
		//reverse skip space chars
		while (*(t - 1) == L' ') {
			t--;
		}

		*t = L'\0';
		

        if (read_redirect_list(start, &list)) {
            item = list.begin;
            while (item) {
#ifdef MS_WINDOWS
                if (fnmatch_w(item->pattern, filename, FNM_CASEFOLD) == 0) {
#else
                if (fnmatch_w(item->pattern, filename, 0) == 0) {
#endif
                    wcscpy(fullfname, start);
                    if (joinpath(fullfname, item->relative_path)) {
                        if (exists(fullfname)) {
                            wcscpy(redirect_filename, fullfname);
                            ret = 1;
                            break;
                        }
                    }
                }
 
                item = item->next;
            }
        }

        if (ret == 0) {
            wcscpy(fullfname, start);
            if (joinpath(fullfname, py_file_name)) {
                if (exists(fullfname)) {
                    wcscpy(redirect_filename, fullfname);
                    ret = 1;
                    break;
                }
            }
        }

        if (end) {
            //skip to next path start
            start = end + 1;
            while (*start == L' ' && *start != L'\0') {
                start++;
            }

            if (*start == L'\0') break;
        }
        else {
            break;
        }
    }

    return ret;
}

int search_hook(const char* filename, char* redirect_filename) {
    int ret;
    wchar_t wfilename[MAXPATHLEN];
    wchar_t wout[MAXPATHLEN];
    char* saved_locale;
    saved_locale = strdup(setlocale(LC_CTYPE, NULL));
    setlocale(LC_CTYPE, "");
    ret = 0;
    if (mbstowcs(wfilename, filename, MAXPATHLEN) != -1) {
        //printf("search_hook %ls\n", wfilename);
        ret = search_hook_w(wfilename, wout);
        if (ret) {
            if (wcstombs(redirect_filename, wout, MAXPATHLEN) == -1) {
                ret = 0;
            }
        }
    }
    setlocale(LC_CTYPE, saved_locale);
    free(saved_locale);

    return ret;
}


FILE* hook_fopen(const char * _FileName, const char * _Mode){
    char filepath[MAXPATHLEN];
    if (search_hook(_FileName,filepath)){
        //printf("hook_fopen %s\n", filepath);
        FILE* file = fopen(filepath, _Mode);
        if (file) return file;
    }
    return fopen(_FileName, _Mode);
}

int hook_open(const char * _FileName, int _OFlag, int _PMode){
    char filepath[MAXPATHLEN];
    if (search_hook(_FileName,filepath)){
        //printf("hook_open %s\n", filepath);
        int fd = open(filepath, _OFlag, _PMode);
        if (fd!=-1) return fd;
    }
    return open(_FileName, _OFlag, _PMode);
}


#ifdef MS_WINDOWS

FILE* hook_wfopen(const wchar_t* _FileName, const wchar_t* _Mode) {
	FILE* file;
    wchar_t filepath[MAXPATHLEN];
    char* saved_locale;
    saved_locale = strdup(setlocale(LC_CTYPE, NULL));
    setlocale(LC_CTYPE, "");
    if (search_hook_w(_FileName, filepath)) {
        //printf("hook %ls\n", filepath);
        setlocale(LC_CTYPE, saved_locale);
        free(saved_locale);
        file = _wfopen(filepath, _Mode);
        if (file) return file;
    }
    setlocale(LC_CTYPE, saved_locale);
    free(saved_locale);
    return _wfopen(_FileName, _Mode);
}

int hook_wopen(const wchar_t* _FileName, int _OFlag, int _PMode) {
	int fd;
    wchar_t filepath[MAXPATHLEN];
    char* saved_locale;
    saved_locale = strdup(setlocale(LC_CTYPE, NULL));
    setlocale(LC_CTYPE, "");
    if (search_hook_w(_FileName, filepath)) {
        //printf("hook %ls\n", filepath);
        setlocale(LC_CTYPE, saved_locale);
        free(saved_locale);
        fd = _wopen(filepath, _OFlag, _PMode);
        if (fd != -1) return fd;
    }
    setlocale(LC_CTYPE, saved_locale);
    free(saved_locale);
    return _wopen(_FileName, _OFlag, _PMode);
}

#endif
