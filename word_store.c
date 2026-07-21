#include "main.h"

/* --- Word storage --- */
WordEntry*  g_words        = NULL;
int         g_word_count   = 0;
int         g_word_capacity = 0;

/* Parse one line of words.txt: word|phonetic|translation */
static int ParseWordLine(wchar_t* line, WordDef* def) {
    /* strip trailing \r\n */
    wchar_t* p = line;
    while (*p && *p != L'\r' && *p != L'\n') p++;
    *p = L'\0';

    wchar_t* seg1 = line;
    wchar_t* seg2 = wcschr(line, L'|');
    if (!seg2) return 0;
    *seg2 = L'\0'; seg2++;

    wchar_t* seg3 = wcschr(seg2, L'|');
    if (!seg3) return 0;
    *seg3 = L'\0'; seg3++;

    wcsncpy(def->word, seg1, MAX_WORD_LEN-1);
    def->word[MAX_WORD_LEN-1] = L'\0';
    wcsncpy(def->phonetic, seg2, MAX_PHONETIC_LEN-1);
    def->phonetic[MAX_PHONETIC_LEN-1] = L'\0';
    wcsncpy(def->translation, seg3, MAX_TRANS_LEN-1);
    def->translation[MAX_TRANS_LEN-1] = L'\0';

    return 1;
}

int LoadWords(const wchar_t* path) {
    /* Count lines first */
    FILE* f = _wfopen(path, L"r, ccs=UTF-8");
    if (!f) {
        /* try relative path from exe */
        wchar_t altPath[MAX_PATH];
        GetExePath(altPath, MAX_PATH);
        wcscat(altPath, L"\\words.txt");
        f = _wfopen(altPath, L"r, ccs=UTF-8");
    }
    if (!f) return 0;

    /* Allocate initial capacity */
    g_word_capacity = 256;
    g_words = (WordEntry*)calloc(g_word_capacity, sizeof(WordEntry));
    g_word_count = 0;

    wchar_t line[MAX_LINE_LEN];
    while (fgetws(line, MAX_LINE_LEN, f) && g_word_count < MAX_WORDS) {
        /* skip empty lines */
        if (line[0] == L'\0' || line[0] == L'\r' || line[0] == L'\n' || line[0] == L'#')
            continue;

        /* grow if needed */
        if (g_word_count >= g_word_capacity) {
            g_word_capacity *= 2;
            if (g_word_capacity > MAX_WORDS) g_word_capacity = MAX_WORDS;
            g_words = (WordEntry*)realloc(g_words, g_word_capacity * sizeof(WordEntry));
            memset(&g_words[g_word_count], 0,
                   (g_word_capacity - g_word_count) * sizeof(WordEntry));
        }

        if (ParseWordLine(line, &g_words[g_word_count].def)) {
            g_word_count++;
        }
    }
    fclose(f);

    /* Init progress for all words */
    InitWordProgress();

    /* Load familiar words (overrides familiar=FALSE from InitWordProgress) */
    {
        wchar_t famPath[MAX_PATH];
        GetFamiliarPath(famPath, MAX_PATH);
        LoadFamiliarWords(famPath);
    }

    return g_word_count;
}

void InitWordProgress(void) {
    for (int i = 0; i < g_word_count; i++) {
        g_words[i].prog.first_seen = 0;
        g_words[i].prog.review_count = 0;
        g_words[i].prog.next_review = 0;    /* 0 = any time / not scheduled */
        g_words[i].prog.last_shown = 0;
        g_words[i].prog.familiar = FALSE;
    }
}

int LoadProgress(const wchar_t* path) {
    FILE* f = _wfopen(path, L"r, ccs=UTF-8");
    if (!f) return 0;  /* no progress file yet, use defaults */

    wchar_t line[MAX_LINE_LEN];
    while (fgetws(line, MAX_LINE_LEN, f)) {
        wchar_t* p = line;
        while (*p && *p != L'\r' && *p != L'\n') p++;
        *p = L'\0';

        wchar_t word_buf[MAX_WORD_LEN] = {0};
        int first_seen = 0, review_count = 0, next_review = 0, last_shown = 0;
        int familiar = 0;
        int n = swscanf(line, L"%[^|]|%d|%d|%d|%d|%d",
                        word_buf, &first_seen, &review_count, &next_review, &last_shown, &familiar);
        if (n < 1) continue;

        /* find matching word */
        for (int i = 0; i < g_word_count; i++) {
            if (wcscmp(g_words[i].def.word, word_buf) == 0) {
                g_words[i].prog.first_seen = first_seen;
                g_words[i].prog.review_count = review_count;
                g_words[i].prog.next_review = next_review;
                g_words[i].prog.last_shown = last_shown;
                /* familiar flag is NOT loaded from progress.dat —
                 * familiar.dat is the authoritative source for familiar status */
                break;
            }
        }
    }
    fclose(f);
    return 1;
}

int SaveProgress(const wchar_t* path) {
    FILE* f = _wfopen(path, L"w, ccs=UTF-8");
    if (!f) return 0;

    for (int i = 0; i < g_word_count; i++) {
        WordProgress* p = &g_words[i].prog;
        fwprintf(f, L"%s|%d|%d|%d|%d|%d\n",
                 g_words[i].def.word,
                 p->first_seen, p->review_count, p->next_review, p->last_shown,
                 p->familiar ? 1 : 0);
    }
    fflush(f);   /* ensure data is flushed to disk before close */
    fclose(f);
    return 1;
}

/* Save to exe directory (alongside words.txt) — no directory creation needed */
void SaveProgressDefault(void) {
    wchar_t progPath[MAX_PATH];
    GetExePath(progPath, MAX_PATH);
    wcscat(progPath, L"\\progress.dat");
    SaveProgress(progPath);
}

/* --- Familiar word persistence (familiar.dat) ---
 * Uses narrow fopen + manual UTF-8 conversion to avoid MinGW _wfopen + ccs=UTF-8 issues. */

/* Get familiar.dat path alongside exe */
void GetFamiliarPath(wchar_t* buf, int buflen) {
    GetExePath(buf, buflen);
    wcscat(buf, L"\\familiar.dat");
}

/* Rewrite entire familiar.dat with all currently-familiar words (UTF-8) */
int SaveFamiliarWords(const wchar_t* path) {
    FILE* f = _wfopen(path, L"wb");  /* binary mode — write raw UTF-8 bytes */
    if (!f) return 0;

    /* Write UTF-8 BOM so Notepad recognizes the encoding */
    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    fwrite(bom, 1, 3, f);

    for (int i = 0; i < g_word_count; i++) {
        if (g_words[i].prog.familiar) {
            char utf8[MAX_WORD_LEN * 4];
            int len = WideCharToMultiByte(CP_UTF8, 0, g_words[i].def.word, -1,
                                          utf8, sizeof(utf8), NULL, NULL);
            if (len > 1) {
                fwrite(utf8, 1, len - 1, f);  /* exclude null terminator */
                fputc('\n', f);
            }
        }
    }
    fflush(f);
    fclose(f);
    return 1;
}

/* Append ONE familiar word to familiar.dat (called immediately on mark) */
int AppendFamiliarWord(const wchar_t* path, const wchar_t* word) {
    FILE* f = _wfopen(path, L"ab");  /* binary append mode */
    if (!f) return 0;

    char utf8[MAX_WORD_LEN * 4];
    int len = WideCharToMultiByte(CP_UTF8, 0, word, -1, utf8, sizeof(utf8), NULL, NULL);
    if (len > 1) {
        fwrite(utf8, 1, len - 1, f);
        fputc('\n', f);
    }
    fflush(f);
    fclose(f);
    return 1;
}

/* Load familiar.dat and set familiar=TRUE for matching words */
int LoadFamiliarWords(const wchar_t* path) {
    /* Read entire file as binary, then parse as UTF-8 */
    FILE* f = _wfopen(path, L"rb");
    if (!f) return 0;  /* no familiar words yet, OK */

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return 0; }

    /* Read entire file into buffer */
    char* buf = (char*)malloc(fsize + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, fsize, f);
    buf[fsize] = '\0';
    fclose(f);

    /* Skip BOM if present */
    char* p = buf;
    if (fsize >= 3 && (unsigned char)p[0] == 0xEF &&
        (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
        p += 3;
    }

    /* Parse lines */
    char* line_start = p;
    while (*line_start) {
        /* Find end of line */
        char* line_end = line_start;
        while (*line_end && *line_end != '\r' && *line_end != '\n')
            line_end++;
        if (line_end == line_start) {
            line_start = (*line_end) ? line_end + 1 : line_end;
            continue;
        }

        /* Null-terminate this line temporarily */
        char saved = *line_end;
        *line_end = '\0';

        /* Convert UTF-8 to wide */
        wchar_t wbuf[MAX_WORD_LEN];
        MultiByteToWideChar(CP_UTF8, 0, line_start, -1, wbuf, MAX_WORD_LEN);

        /* Find matching word and mark familiar */
        for (int i = 0; i < g_word_count; i++) {
            if (wcscmp(g_words[i].def.word, wbuf) == 0) {
                g_words[i].prog.familiar = TRUE;
                break;
            }
        }

        /* Advance to next line */
        *line_end = saved;
        line_start = line_end;
        while (*line_start == '\r' || *line_start == '\n')
            line_start++;
    }

    free(buf);
    return 1;
}
