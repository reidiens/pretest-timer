#include <ncurses.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#define OG_NELEM        32

typedef struct Time {
    int mins;
    int secs;
} tim_t;

typedef struct Time_Record {
    tim_t *tarr;
    int nelem;
    int celem;
} trecord_t;

typedef enum Choices {
    NOP = -1,
    REC,
    VIEWP,      // most recent time
    VIEWA,      // average time
    VIEWPA,     // all previous times
    DEL,
    QUIT,
    NOPTS       // no. of opts in enum (excluding NOP)
} choice_t;

typedef unsigned char uchar;

int init(trecord_t *l);
void pheader(uchar r);
void hrule(int y, float sz);
choice_t query(uchar r);
void answer(void (*cb)(trecord_t *), trecord_t l);
time_t timer(uchar fetch);
int stop_recording(tim_t *r);
int handle_input(choice_t *inp, trecord_t *l, uchar *r, void (**cb)(trecord_t *));
int confirm_del(int celem);
void viewpa(trecord_t *l, void (**cb)(trecord_t *));

void a_rec(trecord_t *l);
void a_viewp(trecord_t *l);
void a_nothing(trecord_t *l);
void a_viewa(trecord_t *l);
void a_del(trecord_t *l);
void a_del_noth(trecord_t *l);

int main() {
    uchar is_recording = 0, running = 1;
    trecord_t list;
    choice_t inp = NOP;
    void (*callback)(trecord_t *) = NULL;
    if (!init(&list)) {
        endwin();
        fprintf(stderr, "Aw, phooey\n"); 
        return -1;
    }
    while (running) {
        pheader(is_recording);
        if (callback != NULL) answer(callback, list);
        inp = query(is_recording); 
        if (inp == QUIT) {
            running = 0;
            continue;
        }
        if (!handle_input(&inp, &list, &is_recording, &callback)) {
            endwin();
            if (list.tarr) free(list.tarr);
            fprintf(stderr, "Aw, shucks\n");
            return -1;
        }
        refresh();
    }
    endwin();
    free(list.tarr);
    return 0;
}

int init(trecord_t *l) {
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);
    l->nelem = OG_NELEM;
    l->celem = 0;
    l->tarr = (tim_t *)malloc(l->nelem * sizeof(tim_t));
    if (!l->tarr) return 0;
    return 1;
}

void pheader(uchar r) {
    erase();
    box(stdscr, 0, 0); 
    int y = 1;
    if (r) {
        attron(A_BOLD | A_STANDOUT);
        mvprintw(y, 1, "*** You're currently recording! ***");
        standend();
        y += 2;
    } 
    mvprintw(y++, 1, "What would you like to do?");
    hrule(y, 0.75);
    refresh();
}

void hrule(int y, float sz) {
    attron(A_BOLD | A_UNDERLINE);
    mvhline(y, 1, '_', (int)((float)(COLS - 2) * sz));
    standend();
}

choice_t query(uchar r) {
    int y = (LINES - 1) - NOPTS; 
    int hy = y;
    int phy = hy;
    hrule(y - 1, 1.0); 
    mvprintw(y++, 1, "%s recording", (r) ? "Stop" : "Start");
    mvprintw(y++, 1, "View previous time");
    mvprintw(y++, 1, "View average time");
    mvprintw(y++, 1, "View all previous times");
    mvprintw(y++, 1, "Delete all previous times");
    mvprintw(y++, 1, "Quit");
    int inp = 0;
    while (1) {
        if (phy != hy) mvchgat(phy, 1, COLS - 2, A_NORMAL, 0, NULL); 
        mvchgat(hy, 1, COLS - 2, A_STANDOUT, 0, NULL);
        switch ((inp = getch())) {
            case KEY_UP:
                if (hy == (LINES - 1) - NOPTS) break;
                phy = hy;
                hy--;
                break;
            case KEY_DOWN:
                if (hy == LINES - 2) break;
                phy = hy;
                hy++;
                break;
            case '\n':
                return hy - ((LINES - 1) - NOPTS);
            default: break;
        } 
    } 
    return -1;
}

void answer(void (*cb)(trecord_t *), trecord_t l) {
    int y = (LINES - 6) - NOPTS;
    move(y, 1);
    attron(A_BOLD);
    cb(&l); 
    standend();
    return;
}

time_t timer(uchar fetch) {
    static time_t t;
    if (!fetch) t = time(NULL);
    else return t;
    return 0;
}

int stop_recording(tim_t *r) {
    time_t tr = time(NULL) - timer(1); 
    struct tm *t = localtime(&tr);
    if (!t) return 0;
    tim_t ret;
    r->mins = t->tm_min;
    r->secs = t->tm_sec;
    return 1;
}

int handle_input(choice_t *inp, trecord_t *l, uchar *r, void (**cb)(trecord_t *)) {
    switch (*inp) {
        case REC:
            if (!(*r)) {
                timer(0);
                *cb = NULL;
                *r = 1;
            }
            else {
                if (l->celem >= (int)((float)l->nelem * 0.6)) {
                    l->nelem *= 2;
                    l->tarr = realloc(l->tarr, l->nelem * sizeof(tim_t)); 
                    if (!l->tarr) return 0;
                }
                if (!stop_recording(&l->tarr[l->celem++])) return 0;
                *cb = a_rec;
                *r = 0;
            }
            break;
        case DEL:
            if (l->celem == 0) {
                *cb = a_del_noth;
                break;
            }
            if (confirm_del(l->celem)) {
                if (l->tarr) free(l->tarr);
                l->nelem = OG_NELEM;
                l->celem = 0;
                l->tarr = (tim_t *)malloc(l->nelem * sizeof(tim_t));
                if (!l->tarr) return -1;
                *cb = a_del;
            } 
            else *cb = NULL;
            break;
        case VIEWPA:
            if (l->celem == 0) *cb = a_nothing;
            else viewpa(l, cb);
            break;
        case VIEWP:
            if (l->celem == 0) *cb = a_nothing;
            else *cb = a_viewp;
            break;
        case VIEWA:
            if (l->celem == 0) *cb = a_nothing;
            else *cb = a_viewa;
            break;
        default: break;
    }
    return 1;
}

int confirm_del(int celem) {
    erase();
    box(stdscr, 0, 0);
    mvprintw(1, 1, "Are you sure you want to delete all %d elements?", celem); 
    int y = LINES - 3;    
    int hy = y;
    int phy = y;
    hrule(y - 1, 1.00);
    mvprintw(y++, 1, "No"); 
    mvprintw(y, 1, "Yes");
    int inp = 0;
    while (1) {
        if (phy != hy) mvchgat(phy, 1, COLS - 2, A_NORMAL, 0, NULL);
        mvchgat(hy, 1, COLS - 2, A_STANDOUT, 0, NULL);
        switch ((inp = getch())) {
            case KEY_UP:
                if (hy == LINES - 3) break;
                phy = hy;
                hy--;
                break;
            case KEY_DOWN:
                if (hy == LINES - 1) break;
                phy = hy;
                hy++;
                break;
            case '\n':
                return hy - (LINES - 3);
        }
    } 
    return 0;
}

void a_rec(trecord_t *l) {
    int m = l->tarr[l->celem - 1].mins;
    int s = l->tarr[l->celem - 1].secs;
    if (m) printw("That pre-test was %d minute%s %d second%slong", m, (m == 1) ? "," : "s,", s, (s == 1) ? " " : "s ");
    else printw("That pre-test was %d second%slong", s, (s == 1) ? " " : "s ");
}

void a_viewp(trecord_t *l) {
    printw("Your previous pre-test was %02d:%02d", l->tarr[l->celem - 1].mins, l->tarr[l->celem - 1].secs); 
}

void a_nothing(trecord_t *l) {
    printw("There are currently no times recorded!"); 
}

void a_viewa(trecord_t *l) {
    int am = 0;
    int as = 0;
    for (int i = 0; i < l->celem; i++) {
        am += l->tarr[i].mins; 
        as += l->tarr[i].secs;
    }
    float tas = as;
    am /= l->celem;
    tas /= l->celem;
    as = (int)tas;
    if (tas - as >= 0.5) as++;
    printw("Your pre-tests take around %02d:%02d, on average", am, as);
}

void a_del(trecord_t *l) {
    printw("Successfully deleted all recorded times");
}

void a_del_noth(trecord_t *l) {
    printw("Nothing to delete!");
}

void display_all_times(tim_t *t, int off, int celem, int maxlines) {
    int y = 1;
    for (int i = 0; i < celem && i < maxlines; i++, y++) {
        mvprintw(y, 1, "%d)", i + off + 1);
        mvprintw(y, 7, "%02d:%02d", t[i + off].mins, t[i + off].secs);
    }
}

void viewpa(trecord_t *l, void (**cb)(trecord_t *)) {
    if (l->celem == 0) {
        *cb = a_nothing;
        return;
    }
    erase();
    box(stdscr, 0, 0);
    int hy = 1, phy = 1, running = 1, inp = 0, i = 0;
    int off = 0;
    int maxlines = LINES - 5;
    while (running) {
        display_all_times(l->tarr, off, l->celem, maxlines); 
        hrule(maxlines + 1, 1.00);
        mvprintw(maxlines + 2, 1, "Ret)\tSelect time");
        mvprintw(maxlines + 3, 1, "Q  )\tExit");
        if (phy != hy) mvchgat(phy, 1, (COLS - 3) / 2, A_NORMAL, 0, NULL);
        mvchgat(hy, 1, (COLS - 3) / 2, A_STANDOUT, 0, NULL);
        switch ((inp = getch())) {
            case 'Q':
            case 'q':
                running = 0;
                continue;
            case KEY_UP:
                if (hy == 1) {
                    if (i > 0) {
                        off--;
                        i--;
                    }
                }
                else {
                    if (hy > 0) {
                        phy = hy;
                        hy--;
                        i--;
                    }
                }
                break;
            case KEY_DOWN:
                if (hy == maxlines) {
                    if (i < l->celem) {
                        off++;
                        i++;
                    }
                }
                else {
                    if (hy < l->celem) {
                        phy = hy;
                        hy++;
                        i++;
                    }
                }
                break; 
            default: break;
        }
        refresh();
    }
}
