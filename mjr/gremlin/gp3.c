#include <dirent.h>
#include <curses.h>
#include <pthread.h>
#include "gt.h"

#define MAX_LIST 1024
#define MAX_WIDTH 512

void curses_init ();
void load_config (int argc, char **argv);
void load_root ();
void refresh_left ();
void refresh_right ();
void update_list ();
void pl_update ();
void * pl_thread (void *args);

void key_up ();
void key_down ();
void key_npage ();
void key_ppage ();
void key_enter ();
void key_left ();
void key_tab ();
void key_backspace ();

char home[256];
int curline, offset;
enum {LEFT, RIGHT};
int focus;

WINDOW *win_left, *win_right, *pad_left, *pad_right;

gremlin_tree *gt;
gt_entity *ls;
char **catalogs;

struct pl_item {
    struct pl_item *next;
    char *name, *key;
    long start_time, bytes_total, bytes_read;
    FILE *data;
};

int pl_len;
struct pl_item *head = NULL, *tail;

void
curses_init ()
{
    initscr(); cbreak(); nonl(); keypad(stdscr, TRUE); curs_set(0); noecho();
    win_left = newwin(LINES, COLS/2, 0, 0);
    win_right = newwin(LINES, COLS/2, 0, COLS/2);
    box(win_left, 0, 0); box(win_right, 0, 0);
    wnoutrefresh(stdscr); wnoutrefresh(win_left); wrefresh(win_right);
    pad_left = newpad(MAX_LIST, MAX_WIDTH);
    pad_right = newpad(MAX_LIST, MAX_WIDTH);
    focus = LEFT; pl_len = 0;
}

void
load_config (int argc, char **argv)
{

}

void
load_root ()
{
    int i, n;
    struct dirent **namelist;
    
    gt = NULL;
    sprintf(home, "%s/.gp3", getenv("HOME"));
   
    if (catalogs) {
	for (i = 0 ; catalogs[i] ; i++) free(catalogs[i]);
	free(catalogs);
    }
    
    n = scandir(home, &namelist, 0, alphasort);
    catalogs = calloc(n - 1, sizeof(char *));
    for (i = 2 ; i < n ; i++) {
	catalogs[i - 2] = strdup(namelist[i]->d_name);
	free(namelist[i]);
    }
    catalogs[n - 2] = NULL;
    free(namelist);
    
    curline = 0;
    offset = 0;
    werase(pad_left);
    if (!catalogs[0]) return;
    wattron(pad_left, A_REVERSE);
    mvwaddstr(pad_left, 0, 0, catalogs[0]);
    wattroff(pad_left, A_REVERSE);
    for (i = 1 ; catalogs[i] ; i++)
        mvwaddstr(pad_left, i, 0, catalogs[i]);
    
    refresh_left();
}

void
update_list ()
{
    int i;
    curline = 0;
    offset = 0;
    werase(pad_left);
    if (!ls[0].name) return;
    wattron(pad_left, A_REVERSE);
    mvwaddstr(pad_left, 0, 0, ls[0].name);
    wattroff(pad_left, A_REVERSE);
    for (i = 1 ; ls[i].name ; i++)
	mvwaddstr(pad_left, i, 0, ls[i].name);
}

void
refresh_right ()
{
    prefresh(pad_right, offset, 0, 1, COLS/2+1, LINES-2, COLS-2);
}

void
refresh_left ()
{
    prefresh(pad_left, offset, 0, 1, 1, LINES-2, COLS/2-2);
}

int
main (int argc, char **argv)
{
    load_config(argc, argv);
    curses_init();
    load_root();
    while (1) {
	switch(getch()) {
	    case KEY_DOWN:      key_down();      break; // scroll down
	    case KEY_UP:        key_up();        break; // scroll up
	    case KEY_NPAGE:     key_npage();     break; // page down
	    case KEY_PPAGE:     key_ppage();     break; // page up
	    case 13:
	    case KEY_RIGHT:     key_enter();     break; // select
	    case KEY_LEFT:      key_left();      break; // go back
	    case '\t':          key_tab();       break; // switch focus
	    case KEY_BACKSPACE: key_backspace(); break; // remove plist entry
	    default: break;
	}
    }
    clear(); endwin();
    return 0;
}

void
key_up ()
{
    if (curline == 0) return;
    if (focus == LEFT) {
        if (!gt) { // root catalogs
    	    mvwaddstr(pad_left, curline, 0, catalogs[curline]);
            wattron(pad_left, A_REVERSE);
            curline--;
            mvwaddstr(pad_left, curline, 0, catalogs[curline]);
            wattroff(pad_left, A_REVERSE);
        } else { // in the tree
	    mvwaddstr(pad_left, curline, 0, ls[curline].name);
            wattron(pad_left, A_REVERSE);
            curline--;
            mvwaddstr(pad_left, curline, 0, ls[curline].name);
            wattroff(pad_left, A_REVERSE);
        }
        if (curline == offset - 1) offset--;
    	refresh_left();
    } else {
	if (--curline == offset - 1) offset--;
	pl_update();
    }
}

void
key_down ()
{
    if (focus == LEFT) {
        if (!gt) { // root catalogs
            if (!catalogs[curline+1]) return;
            mvwaddstr(pad_left, curline, 0, catalogs[curline]);
            wattron(pad_left, A_REVERSE);
            curline++;
            mvwaddstr(pad_left, curline, 0, catalogs[curline]);
            wattroff(pad_left, A_REVERSE);
        } else { // in the tree
	    if (!ls[curline+1].name) return;
	    mvwaddstr(pad_left, curline, 0, ls[curline].name);
            wattron(pad_left, A_REVERSE);
            curline++;
            mvwaddstr(pad_left, curline, 0, ls[curline].name);
            wattroff(pad_left, A_REVERSE);
        }
        if (curline - offset == LINES - 2) offset++;
        refresh_left();
    } else {
	if (curline >= pl_len - 1) return;
	if (++curline - offset == LINES - 2) offset++;
	pl_update();
    }
}

void
key_npage ()
{
    mvwaddstr(pad_left, curline, 0, ls[curline].name);
    offset += LINES - 2;
    curline += LINES - 2;
    refresh_left();
}

void
key_ppage ()
{
    mvwaddstr(pad_left, curline, 0, ls[curline].name);
    offset -= LINES - 2;
    curline -= LINES - 2;
    refresh_left();
}

void
key_enter ()
{
    FILE *in;
    int status;
    char path[256];
    if (focus == RIGHT) return; // this means nothing for playlist stuff
    if (!gt) { // open a catalog
	sprintf(path, "%s/%s", home, catalogs[curline]);
	in = fopen(path, "r");
	if (!in) {
	    printf("Can't open catalog %s!\n", catalogs[curline]);
	    return;
	}
	gt = malloc(sizeof(gremlin_tree));
	status = gt_init(gt, in);
	if (status != 0) {
	    printf("Invalid catalog: %s!\n", catalogs[curline]);
	    free(gt); gt = NULL; return;
	}
	ls = gt_ls(gt);
	update_list();
	refresh_left();
    } else {
	if (ls[curline].type == GT_DIR) { // cd
	    gt_cd(gt, ls[curline].name, ls[curline].data);
	    gt_free(ls); ls = gt_ls(gt);
	    update_list();
	    refresh_left();
	} else { // add to playlist
	    pthread_t t;
	    struct pl_item *new = malloc(sizeof(struct pl_item));
	    new->name = strdup(ls[curline].name); new->key = strdup(ls[curline].data);
	    new->start_time = time(NULL); new->bytes_total = 0; new->bytes_read = 0;
	    new->data = NULL; new->next = NULL;
	    if (!head) { // first node
		head = tail = new;
	    } else {
		tail->next = new;
	        tail = new;
	    }
	    pl_len++;
	    pl_update();
	    pthread_create(&t, NULL, pl_thread, (void *) new);
	}
    }
}

void
key_left ()
{
    if (focus == RIGHT) return; // nope
    if (!gt) return; // already at root
    if (gt->depth == 0) { // cd to root
	load_root();
    } else {
	gt_cd(gt, "..", NULL);
        gt_free(ls); ls = gt_ls(gt);
	update_list();
    }
    refresh_left();
}

void
key_tab ()
{
    if (focus == LEFT) {
	mvwaddstr(pad_left, curline, 0, gt ? ls[curline].name : catalogs[curline]);
	focus = RIGHT;
	curline = 0; offset = 0;
	refresh_left();
	pl_update();
    } else {
        curline = 0; offset = 0;
	wattron(pad_left, A_REVERSE);
	mvwaddstr(pad_left, curline, 0, gt ? ls[curline].name : catalogs[curline]);
	wattroff(pad_left, A_REVERSE);
	focus = LEFT;
	refresh_left();
	pl_update();
    }
}

void
key_backspace ()
{
    int i = 0;
    struct pl_item *last = NULL, *pli = head;
    if (focus == LEFT) return; // nope, sorry
    while (pli) {
	if (i == curline) {
	    if (last) last->next = pli->next;
	    else head = pli->next;
	    if (i == --pl_len) curline--;
	    pl_update();
	    return;
	}
	last = pli;
	pli = pli->next;
	i++;
    }
}

void
pl_update ()
{
    int i = 0;
    char line[512];
    struct pl_item *pli = head;
    werase(pad_right);
    while (pli) {
	sprintf(line, "%s", pli->name);
	if (focus == RIGHT && i == curline) wattron(pad_right, A_REVERSE);
	mvwaddstr(pad_right, i, 0, line);
	if (focus == RIGHT && i == curline) wattroff(pad_right, A_REVERSE);
	pli = pli->next;
	i++;
    }
    refresh_right();
}

void *
pl_thread (void *args)
{
/*    struct pl_item *pli = (struct pl_item *) args;
    char command[512];
    sprintf(command, "mpg123 freenet:CHK@%s &>/dev/null", pli->key);
    system(command);
    head = pli->next;
    free(pli->name); free(pli->key); free(pli);
    pl_len--;
    pl_update();*/
}
