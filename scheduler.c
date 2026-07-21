#include "main.h"

/* Score a single word for selection. Higher = more likely to be picked.
 * - "New" words (never seen): decent priority
 * - "Due" words (next_review <= today): highest priority
 * - "Not due" words: low but non-zero priority
 * - "Already shown today": zero priority
 */
static int ScoreWord(WordEntry* entry, int today) {
    WordProgress* p = &entry->prog;

    /* Familiar words are never shown again */
    if (p->familiar)
        return -1000;  /* impossible to pick */

    /* Don't show same word twice on the same day, unless all words exhausted */
    if (p->last_shown == today && p->review_count > 0)
        return -1;  /* nearly impossible to pick */

    /* Never shown before → good candidate */
    if (p->first_seen == 0)
        return 50;

    /* Due for review → highest priority */
    if (p->next_review > 0 && p->next_review <= today)
        return 100;

    /* Not due yet → low priority, but higher the closer to due date */
    if (p->next_review > 0) {
        int days_until_due = p->next_review - today;
        if (days_until_due > 30) return 1;
        return 30 - days_until_due;  /* 29..1 */
    }

    /* Fallback */
    return 10;
}

/* Quick sort by score descending, with randomization for same scores */
/* For simplicity we use a weighted random pick approach */

/* Pick up to max_count words and store pointers in selected[].
 * Returns actual number picked. */
int PickWords(WordEntry** selected, int max_count) {
    if (g_word_count == 0 || max_count <= 0) return 0;

    int today = TodayDays();
    int* scores = (int*)calloc((size_t)g_word_count, sizeof(int));
    int total_score = 0;

    /* Calculate scores */
    for (int i = 0; i < g_word_count; i++) {
        scores[i] = ScoreWord(&g_words[i], today);
        if (scores[i] < 0) scores[i] = 0;  /* same-day already shown → 0 */
        total_score += scores[i];
    }

    /* If all words have 0 score (all shown today), reset for today */
    if (total_score == 0) {
        for (int i = 0; i < g_word_count; i++) {
            scores[i] = (g_words[i].prog.first_seen == 0) ? 50 :
                        ((g_words[i].prog.next_review > 0 && g_words[i].prog.next_review <= today) ? 100 : 10);
            total_score += scores[i];
        }
    }

    int picked = 0;
    int* already_picked = (int*)calloc((size_t)g_word_count, sizeof(int));

    /* Weighted random selection */
    srand((unsigned)time(NULL) ^ (unsigned)GetTickCount());

    for (int round = 0; round < max_count && total_score > 0; round++) {
        int r = rand() % total_score;
        int cumulative = 0;
        int idx = -1;

        for (int i = 0; i < g_word_count; i++) {
            if (already_picked[i]) continue;
            cumulative += scores[i];
            if (r < cumulative) {
                idx = i;
                break;
            }
        }

        if (idx < 0) {
            /* fallback: pick first unpicked */
            for (int i = 0; i < g_word_count; i++) {
                if (!already_picked[i]) { idx = i; break; }
            }
        }

        if (idx < 0) break; /* all picked already */

        already_picked[idx] = 1;
        selected[picked++] = &g_words[idx];
        total_score -= scores[idx];
    }

    free(already_picked);
    free(scores);
    return picked;
}

/* Update progress after displaying a word */
void UpdateWordProgress(WordEntry* entry) {
    int today = TodayDays();
    WordProgress* p = &entry->prog;

    if (p->first_seen == 0) {
        p->first_seen = today;
        p->review_count = 1;
        p->next_review = today + EB_INTERVALS[1];  /* 1 day later */
    } else {
        p->review_count++;
        int idx = p->review_count;
        if (idx >= EB_INTERVALS_COUNT) idx = EB_INTERVALS_COUNT - 1;
        p->next_review = today + EB_INTERVALS[idx];
    }

    p->last_shown = today;
}
