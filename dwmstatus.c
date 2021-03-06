/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

// internet
#include <net/if.h>

#include <X11/Xlib.h>

char *tzargentina = "America/Buenos_Aires";
char *tzla = "America/Los_Angeles";
char *tzutc = "UTC";
char *tzberlin = "Europe/Berlin";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
	char *co, status;
	int descap, remcap;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "charge_full_design");
	if (co == NULL) {
		co = readfile(base, "energy_full_design");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &descap);
	free(co);

	co = readfile(base, "charge_now");
	if (co == NULL) {
		co = readfile(base, "energy_now");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &remcap);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
	} else {
		status = '?';
	}

	free(co);

	if (remcap < 0 || descap < 0)
		return smprintf("invalid");

	return smprintf("%.0f%%%c", ((float)remcap / (float)descap) * 100, status);
}

char *
getnetwork(void) {
	struct if_nameindex *if_array, *i;
	char *co;
	char *base;
	int networkup;
	char *networkname = NULL;

	if_array = if_nameindex();

	if (if_array == NULL) {
		perror("if_nameindex");
		exit(1);
	}

	for (i = if_array; !(i->if_index == 0 && i->if_name == NULL); ++i) {
		// read corresponding network interface operstate
		base = smprintf("/sys/class/net/%s", i->if_name);
		co = readfile(base, "operstate");
		free(base);
		if (co == NULL) continue;
		networkup = (strncmp(co, "up", 2) == 0);
		free(co);
		if (networkup) {
			networkname = smprintf("%s", i->if_name);
			break;
		}
	}
	if_freenameindex(if_array);

	if (!networkname) return smprintf("");

	return networkname;
}

int
main(void)
{
	char *status;
	char *bat;
	char *tmla;
	char *network;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(60)) {
		bat = getbattery("/sys/class/power_supply/BAT0");
		tmla = mktimes("%a %b %d %Y %H:%M %Z", tzla);
		network = getnetwork();

		status = smprintf("\U0001F578%s \U0001F50B%s \U0001F4C6%s",
			          network, bat, tmla);
		//status = smprintf("Network:%s Battery:%s Calendar: %s", network, bat, tmla);
		setstatus(status);

		free(bat);
		free(tmla);
		free(network);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

