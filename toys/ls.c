/* vi: set sw=4 ts=4:
 *
 * ls.c - list files
 *
 * Copyright 2012 Andre Renaud <andre@bluewatersys.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/ls.html

USE_LS(NEWTOY(ls, "lF1a", TOYFLAG_BIN))

config LS
	bool "ls"
	default y
	help
	  usage: ls [-l] [-F] [-a] [-1] [directory...]
	  list files

          -F    append a character as a file type indicator
          -a    list all files
          -1    list one file per line
          -l    show full details for each file
*/

#include "toys.h"

#define FLAG_a 1
#define FLAG_1 2
#define FLAG_F 4
#define FLAG_l 8

static int dir_filter(const struct dirent *d)
{
    /* Skip over the . & .. entries unless -a is given */
    if (!(toys.optflags & FLAG_a))
        if (d->d_name[0] == '.')
            return 0;
    return 1;
}

static void do_ls(int fd, char *name)
{
    struct dirent **entries;
    int nentries;
    int i;
    int maxwidth = -1;
    int ncolumns = 1;

    if (!name || strcmp(name, "-") == 0)
        name = ".";

    /* Get all the files in this directory */
    nentries = scandir(name, &entries, dir_filter, alphasort);
    if (nentries < 0)
        perror_exit("ls: cannot access %s'", name);


    /* Determine the widest entry so we can flow them properly */
    if (!(toys.optflags & FLAG_1)) {
        int columns;
        char *columns_str;

        for (i = 0; i < nentries; i++) {
            struct dirent *ent = entries[i];
            int width;

            width = strlen(ent->d_name);
            if (width > maxwidth)
                maxwidth = width;
        }
        /* We always want at least a single space for each entry */
        maxwidth++;
        if (toys.optflags & FLAG_F)
            maxwidth++;

        columns_str = getenv("COLUMNS");
        columns = columns_str ? atoi(columns_str) : 80;
        ncolumns = maxwidth ? columns / maxwidth : 1;
    }

    for (i = 0; i < nentries; i++) {
        struct dirent *ent = entries[i];
        int len = strlen(ent->d_name);
        struct stat st;
        int stat_valid = 0;

        /* Provide the ls -l long output */
        if (toys.optflags & FLAG_l) {
            char type;
            char timestamp[64];
            struct tm mtime;

            if (lstat(ent->d_name, &st))
                perror_exit("Can't stat %s", ent->d_name);
            stat_valid = 1;
            if (S_ISDIR(st.st_mode))
                type = 'd';
            else if (S_ISCHR(st.st_mode))
                type = 'c';
            else if (S_ISBLK(st.st_mode))
                type = 'b';
            else if (S_ISLNK(st.st_mode))
                type = 'l';
            else
                type = '-';

            xprintf("%c%c%c%c%c%c%c%c%c%c ", type,
                (st.st_mode & S_IRUSR) ? 'r' : '-',
                (st.st_mode & S_IWUSR) ? 'w' : '-',
                (st.st_mode & S_IXUSR) ? 'x' : '-',
                (st.st_mode & S_IRGRP) ? 'r' : '-',
                (st.st_mode & S_IWGRP) ? 'w' : '-',
                (st.st_mode & S_IXGRP) ? 'x' : '-',
                (st.st_mode & S_IROTH) ? 'r' : '-',
                (st.st_mode & S_IWOTH) ? 'w' : '-',
                (st.st_mode & S_IXOTH) ? 'x' : '-');

            xprintf("%2d ", st.st_nlink);
            /* FIXME: We're never looking up uid/gid numbers */
            xprintf("%4d ", st.st_uid);
            xprintf("%4d ", st.st_gid);
            if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
                xprintf("%3d, %3d ", major(st.st_rdev), minor(st.st_rdev));
            else
                xprintf("%8ld ", st.st_size);

            localtime_r(&st.st_mtime, &mtime);

            strftime(timestamp, sizeof(timestamp), "%b %e %H:%M", &mtime);
            xprintf("%s ", timestamp);
        }

        xprintf("%s", ent->d_name);

        /* Append the file-type indicator character */
        if (toys.optflags & FLAG_F) {
            if (!stat_valid) {
                if (lstat(ent->d_name, &st))
                    perror_exit("Can't stat %s", ent->d_name);
                stat_valid = 1;
            }
            if (S_ISDIR(st.st_mode)) {
                xprintf("/");
                len++;
            } else if (S_ISREG(st.st_mode) &&
                (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
                xprintf("*");
                len++;
            } else if (S_ISLNK(st.st_mode)) {
                xprintf("@");
                len++;
            }
        }
        if (toys.optflags & FLAG_1) {
            xprintf("\n");
        } else {
            if (i % ncolumns == ncolumns - 1)
                xprintf("\n");
            else
                xprintf("%*s", maxwidth - len, "");
        }
    }
    /* Make sure we put at a trailing new line in */
    if (!(toys.optflags & FLAG_1) && (i % ncolumns))
        xprintf("\n");
}

void ls_main(void)
{
    /* Long output must be one-file per line */
    if (toys.optflags & FLAG_l)
        toys.optflags |= FLAG_1;
    loopfiles(toys.optargs, do_ls);
}