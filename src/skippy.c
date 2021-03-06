/* Skippy - Seduces Kids Into Perversion
 *
 * Copyright (C) 2004 Hyriand <hyriand@thegraveyard.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "skippy.h"
#include <errno.h>
#include <locale.h>
#include <getopt.h>
#include <strings.h>
#include <limits.h>

session_t *ps_g = NULL;

static int DIE_NOW = 0;

static dlist *
update_clients(MainWin *mw, dlist *clients, Bool *touched)
{
	dlist *stack, *iter;
	
	stack = dlist_first(wm_get_stack(mw->dpy));
	iter = clients = dlist_first(clients);
	
	if(touched)
		*touched = False;
	
	/* Terminate clients that are no longer managed */
	while(iter)
	{
		ClientWin *cw = (ClientWin *)iter->data;
		if(! dlist_find_data(stack, (void *)cw->client.window))
		{
			dlist *tmp = iter->next;
			clientwin_destroy((ClientWin *)iter->data, True);
			clients = dlist_remove(iter);
			iter = tmp;
			if(touched)
				*touched = True;
			continue;
		}
		clientwin_update(cw);
		iter = iter->next;
	}
	
	/* Add new clients */
	for(iter = dlist_first(stack); iter; iter = iter->next)
	{
		ClientWin *cw = (ClientWin*)dlist_find(clients, clientwin_cmp_func, iter->data);
		if(! cw && (Window)iter->data != mw->window)
		{
			cw = clientwin_create(mw, (Window)iter->data);
			if (!cw) continue;
			clients = dlist_add(clients, cw);
			clientwin_update(cw);
			if(touched)
				*touched = True;
		}
	}
	
	dlist_free(stack);
	
	return clients;
}

static dlist *
do_layout(MainWin *mw, dlist *clients, Window focus, Window leader)
{
	session_t * const ps = mw->ps;

	CARD32 desktop = wm_get_current_desktop(mw->dpy);
	unsigned int width, height;
	float factor;
	int xoff, yoff;
	dlist *iter, *tmp;
	
	/* Update the client table, pick the ones we want and sort them */
	clients = update_clients(mw, clients, 0);
	
	if(mw->cod)
		dlist_free(mw->cod);
	
	tmp = dlist_first(dlist_find_all(clients, (dlist_match_func)clientwin_validate_func, &desktop));
	if(leader != None)
	{
		mw->cod = dlist_first(dlist_find_all(tmp, clientwin_check_group_leader_func, (void*)&leader));
		dlist_free(tmp);
	} else
		mw->cod = tmp;
	
	if(! mw->cod)
		return clients;
	
	dlist_sort(mw->cod, clientwin_sort_func, 0);
	
	/* Move the mini windows around */
	layout_run(mw, mw->cod, &width, &height);
	factor = (float)(mw->width - 100) / width;
	if(factor * height > mw->height - 100)
		factor = (float)(mw->height - 100) / height;
	
	xoff = (mw->width - (float)width * factor) / 2;
	yoff = (mw->height - (float)height * factor) / 2;
	mainwin_transform(mw, factor);
	for(iter = mw->cod; iter; iter = iter->next)
		clientwin_move((ClientWin*)iter->data, factor, xoff, yoff);
	
	/* Get the currently focused window and select which mini-window to focus */
	iter = dlist_find(mw->cod, clientwin_cmp_func, (void *)focus);
	if(! iter)
		iter = mw->cod;
	mw->focus = (ClientWin*)iter->data;
	mw->focus->focused = 1;
	
	/* Map the client windows */
	for(iter = mw->cod; iter; iter = iter->next)
		clientwin_map((ClientWin*)iter->data);
	if (ps->o.movePointerOnStart)
		XWarpPointer(mw->dpy, None, mw->focus->mini.window, 0, 0, 0, 0,
				mw->focus->mini.width / 2, mw->focus->mini.height / 2);
	
	return clients;
}

static dlist *
skippy_run(MainWin *mw, dlist *clients, Window focus, Window leader, Bool all_xin)
{
	session_t * const ps = mw->ps;
	XEvent ev;
	int die = 0;
	Bool refocus = False;
	int last_rendered;
	
	/* Update the main window's geometry (and Xinerama info if applicable) */
	mainwin_update(mw);
#ifdef CFG_XINERAMA
	if(all_xin)
	{
		mw->xin_active = 0;
	}
#endif /* CFG_XINERAMA */
	
	/* Map the main window and run our event loop */
	if(mw->lazy_trans)
	{
		mainwin_map(mw);
		XFlush(mw->dpy);
	}
	
	clients = do_layout(mw, clients, focus, leader);
	if(! mw->cod)
		return clients;
	
	/* Map the main window and run our event loop */
	if(! mw->lazy_trans)
		mainwin_map(mw);
	
	last_rendered = time_in_millis();
	while(! die) {
		int i, j, now, timeout;
		int move_x = -1, move_y = -1;
		struct pollfd r_fd;

		XFlush(mw->dpy);

		r_fd.fd = ConnectionNumber(mw->dpy);
		r_fd.events = POLLIN;
		if(mw->poll_time > 0)
			timeout = MAX(0, mw->poll_time + last_rendered - time_in_millis());
		else
			timeout = -1;
		i = poll(&r_fd, 1, timeout);

		now = time_in_millis();
		if(now >= last_rendered + mw->poll_time)
		{
			REDUCE(if( ((ClientWin*)iter->data)->damaged ) clientwin_repair(iter->data), mw->cod);
			last_rendered = now;
		}

		i = XPending(mw->dpy);
		for(j = 0; j < i; ++j)
		{
			XNextEvent(mw->dpy, &ev);

			if (ev.type == MotionNotify)
			{
				move_x = ev.xmotion.x_root;
				move_y = ev.xmotion.y_root;
			}
			else if (ev.type == DestroyNotify || ev.type == UnmapNotify) {
				dlist *iter = dlist_find(clients, clientwin_cmp_func, (void *)ev.xany.window);
				if(iter)
				{
					ClientWin *cw = (ClientWin *)iter->data;
					clients = dlist_first(dlist_remove(iter));
					iter = dlist_find(mw->cod, clientwin_cmp_func, (void *)ev.xany.window);
					if(iter)
						mw->cod = dlist_first(dlist_remove(iter));
					clientwin_destroy(cw, True);
					if(! mw->cod)
					{
						die = 1;
						break;
					}
				}
			}
			else if (mw->poll_time >= 0 && ev.type == ps->xinfo.damage_ev_base + XDamageNotify)
			{
				XDamageNotifyEvent *d_ev = (XDamageNotifyEvent *)&ev;
				dlist *iter = dlist_find(mw->cod, clientwin_cmp_func, (void *)d_ev->drawable);
				if(iter)
				{
					if(mw->poll_time == 0)
						clientwin_repair((ClientWin *)iter->data);
					else
						((ClientWin *)iter->data)->damaged = true;
				}

			}
			else if(ev.type == KeyRelease && ev.xkey.keycode == mw->key_q)
			{
				DIE_NOW = 1;
				die = 1;
				break;
			}
			else if(ev.type == KeyRelease && ev.xkey.keycode == mw->key_escape)
			{
				refocus = True;
				die = 1;
				break;
			}
			else if(ev.xany.window == mw->window)
				die = mainwin_handle(mw, &ev);
			else if(ev.type == PropertyNotify)
			{
				if(ev.xproperty.atom == ESETROOT_PMAP_ID || ev.xproperty.atom == _XROOTPMAP_ID)
				{
					mainwin_update_background(mw);
					REDUCE(clientwin_render((ClientWin *)iter->data), mw->cod);
				}

			}
			else if(mw->tooltip && ev.xany.window == mw->tooltip->window)
				tooltip_handle(mw->tooltip, &ev);
			else
			{
				dlist *iter;
				for(iter = mw->cod; iter; iter = iter->next)
				{
					ClientWin *cw = (ClientWin *)iter->data;
					if(cw->mini.window == ev.xany.window)
					{
						die = clientwin_handle(cw, &ev);
						if(die)
							break;
					}
				}
				if(die)
					break;
			}
		}

		if(mw->tooltip && move_x != -1)
			tooltip_move(mw->tooltip, move_x + 20, move_y + 20);
	}
	
	/* Unmap the main window and clean up */
	mainwin_unmap(mw);
	XFlush(mw->dpy);
	
	REDUCE(clientwin_unmap((ClientWin*)iter->data), mw->cod);
	dlist_free(mw->cod);
	mw->cod = 0;
	
	if(die == 2)
		DIE_NOW = 1;
	
	if(refocus)
	{
		XSetInputFocus(mw->dpy, focus, RevertToPointerRoot, CurrentTime);
	}
	
	return clients;
}

void send_command_to_daemon_via_fifo(int command, const char* pipePath)
{
	FILE *fp;
	if (access(pipePath, W_OK) != 0)
	{
		fprintf(stderr, "pipe does not exist, exiting...\n");
		exit(1);
	}
	
	fp = fopen(pipePath, "w");
	
	printf("sending command...\n");
	fputc(command, fp);
	
	fclose(fp);
}

void activate_window_picker(const char* pipePath)
{
	send_command_to_daemon_via_fifo(ACTIVATE_WINDOW_PICKER, pipePath);
}

void exit_daemon(const char* pipePath)
{
	send_command_to_daemon_via_fifo(EXIT_RUNNING_DAEMON, pipePath);
}

/**
 * Xlib error handler function.
 */
static int
xerror(Display *dpy, XErrorEvent *ev) {
	session_t * const ps = ps_g;

	int o;
	const char *name = "Unknown";

#define CASESTRRET2(s)	 case s: name = #s; break

	o = ev->error_code - ps->xinfo.fixes_err_base;
	switch (o) {
		CASESTRRET2(BadRegion);
	}

	o = ev->error_code - ps->xinfo.damage_err_base;
	switch (o) {
		CASESTRRET2(BadDamage);
	}

	o = ev->error_code - ps->xinfo.render_err_base;
	switch (o) {
		CASESTRRET2(BadPictFormat);
		CASESTRRET2(BadPicture);
		CASESTRRET2(BadPictOp);
		CASESTRRET2(BadGlyphSet);
		CASESTRRET2(BadGlyph);
	}

	switch (ev->error_code) {
		CASESTRRET2(BadAccess);
		CASESTRRET2(BadAlloc);
		CASESTRRET2(BadAtom);
		CASESTRRET2(BadColor);
		CASESTRRET2(BadCursor);
		CASESTRRET2(BadDrawable);
		CASESTRRET2(BadFont);
		CASESTRRET2(BadGC);
		CASESTRRET2(BadIDChoice);
		CASESTRRET2(BadImplementation);
		CASESTRRET2(BadLength);
		CASESTRRET2(BadMatch);
		CASESTRRET2(BadName);
		CASESTRRET2(BadPixmap);
		CASESTRRET2(BadRequest);
		CASESTRRET2(BadValue);
		CASESTRRET2(BadWindow);
	}

#undef CASESTRRET2

	print_timestamp(ps);
	{
		char buf[BUF_LEN] = "";
		XGetErrorText(ps->dpy, ev->error_code, buf, BUF_LEN);
		printf("error %d (%s) request %d minor %d serial %lu (\"%s\")\n",
				ev->error_code, name, ev->request_code,
				ev->minor_code, ev->serial, buf);
	}

	return 0;
}

#ifndef SKIPPYXD_VERSION
#define SKIPPYXD_VERSION "unknown"
#endif

void show_help()
{
	fputs("skippy-xd (" SKIPPYXD_VERSION ")\n"
			"Usage: skippy-xd [command]\n\n"
			"The available commands are:\n"
			"\t--start-daemon            - starts the daemon running.\n"
			"\t--stop-daemon             - stops the daemon running.\n"
			"\t--activate-window-picker  - tells the daemon to show the window picker.\n"
			"\t--help                    - show this message.\n"
			"\t-S                        - Synchronize X operation (debugging).\n"
			, stdout);
}

static inline bool
init_xexts(session_t *ps) {
	Display * const dpy = ps->dpy;
#ifdef CFG_XINERAMA
	ps->xinfo.xinerama_exist = XineramaQueryExtension(dpy,
			&ps->xinfo.xinerama_ev_base, &ps->xinfo.xinerama_err_base);
# ifdef DEBUG_XINERAMA
	printfef("(): Xinerama extension: %s",
			(ps->xinfo.xinerama_exist ? "yes": "no"));
# endif /* DEBUG_XINERAMA */
#endif /* CFG_XINERAMA */

	if(!XDamageQueryExtension(dpy,
				&ps->xinfo.damage_ev_base, &ps->xinfo.damage_err_base)) {
		printfef("(): FATAL: XDamage extension not found.");
		return false;
	}

	if(!XCompositeQueryExtension(dpy, &ps->xinfo.composite_ev_base,
				&ps->xinfo.composite_err_base)) {
		printfef("(): FATAL: XComposite extension not found.");
		return false;
	}

	if(!XRenderQueryExtension(dpy,
				&ps->xinfo.render_ev_base, &ps->xinfo.render_err_base)) {
		printfef("(): FATAL: XRender extension not found.");
		return false;
	}

	if(!XFixesQueryExtension(dpy,
				&ps->xinfo.fixes_ev_base, &ps->xinfo.fixes_err_base)) {
		printfef("(): FATAL: XFixes extension not found.");
		return false;
	}

	return true;
}

/**
 * @brief Check if a file exists.
 *
 * access() may not actually be reliable as according to glibc manual it uses
 * real user ID instead of effective user ID, but stat() is just too costly
 * for this purpose.
 */
static inline bool
fexists(const char *path) {
	return !access(path, F_OK);
}

/**
 * @brief Find path to configuration file.
 */
static inline char *
get_cfg_path(void) {
	static const char *PATH_CONFIG_HOME_SUFFIX = "/skippy-xd/skippy-xd.rc";
	static const char *PATH_CONFIG_HOME = "/.config";
	static const char *PATH_CONFIG_SYS_SUFFIX = "/skippy-xd.rc";
	static const char *PATH_CONFIG_SYS = "/etc/xdg";

	char *path = NULL;
	const char *dir = NULL;

	// Check $XDG_CONFIG_HOME
	if ((dir = getenv("XDG_CONFIG_HOME")) && strlen(dir)) {
		path = mstrjoin(dir, PATH_CONFIG_HOME_SUFFIX);
		if (fexists(path))
			goto get_cfg_path_found;
		free(path);
		path = NULL;
	}
	// Check ~/.config
	if ((dir = getenv("HOME")) && strlen(dir)) {
		path = mstrjoin3(dir, PATH_CONFIG_HOME, PATH_CONFIG_HOME_SUFFIX);
		if (fexists(path))
			goto get_cfg_path_found;
		free(path);
		path = NULL;
	}
	// Check $XDG_CONFIG_DIRS
	if (!((dir = getenv("XDG_CONFIG_DIRS")) && strlen(dir)))
		dir = PATH_CONFIG_SYS;
	{
		char *dir_free = mstrdup(dir);
		char *part = strtok(dir_free, ":");
		while (part) {
			path = mstrjoin(part, PATH_CONFIG_SYS_SUFFIX);
			if (fexists(path)) {
				free(dir_free);
				goto get_cfg_path_found;
			}
			free(path);
			path = NULL;
			part = strtok(NULL, ":");
		}
		free(dir_free);
	}

	return NULL;

get_cfg_path_found:
	return path;
}

int main(int argc, char *argv[])
{
	session_t *ps = NULL;
	int ret = RET_SUCCESS;

	dlist *clients = NULL;
	Display *dpy = NULL;
	MainWin *mw = NULL;
	Bool invertShift = False;
	Bool runAsDaemon = False;
	Window leader, focused;
	int result;
	int flush_file = 0;
	FILE *fp;
	int piped_input;
	int exitDaemon = 0;
	bool synchronize = false;
	
	/* Set program locale */
	setlocale (LC_ALL, "");

	// Initialize session structure
	{
		static const session_t SESSIONT_DEF = SESSIONT_INIT;
		ps_g = ps = allocchk(malloc(sizeof(session_t)));
		memcpy(ps, &SESSIONT_DEF, sizeof(session_t));
		gettimeofday(&ps->time_start, NULL);
	}

	// Load configuration file
	{
		dlist *config = NULL;
		{
			char *path = get_cfg_path();
			if (path) {
				config = config_load(path);
				free(path);
			}
			else
				printfef("(): WARNING: No configuration file found.");
		}

		// Read configuration into ps->o, because searching all the time is much
		// less efficient, may introduce inconsistent default value, and
		// occupies a lot more memory for non-string types.
		ps->o.pipePath = mstrdup(config_get(config, "general", "pipePath", "/tmp/skippy-xd-fifo"));
		ps->o.normal_tint = mstrdup(config_get(config, "normal", "tint", "black"));
		ps->o.highlight_tint = mstrdup(config_get(config, "highlight", "tint", "#101020"));
		ps->o.tooltip_border = mstrdup(config_get(config, "tooltip", "border", "#e0e0e0"));
		ps->o.tooltip_background = mstrdup(config_get(config, "tooltip", "background", "#404040"));
		ps->o.tooltip_text = mstrdup(config_get(config, "tooltip", "text", "#e0e0e0"));
		ps->o.tooltip_textShadow = mstrdup(config_get(config, "tooltip", "textShadow", "black"));
		ps->o.tooltip_font = mstrdup(config_get(config, "tooltip", "font", "fixed-11:weight=bold"));
		if (config) {
			config_get_int_wrap(config, "general", "distance", &ps->o.distance, 1, INT_MAX);
			config_get_bool_wrap(config, "general", "useNetWMFullscreen", &ps->o.useNetWMFullscreen);
			config_get_bool_wrap(config, "general", "ignoreSkipTaskbar", &ps->o.ignoreSkipTaskbar);
			config_get_double_wrap(config, "general", "updateFreq", &ps->o.updateFreq, -1000.0, 1000.0);
			config_get_bool_wrap(config, "general", "lazyTrans", &ps->o.lazyTrans);
			config_get_bool_wrap(config, "general", "useNameWindowPixmap", &ps->o.useNameWindowPixmap);
			config_get_bool_wrap(config, "general", "movePointerOnStart", &ps->o.movePointerOnStart);
			config_get_bool_wrap(config, "xinerama", "showAll", &ps->o.xinerama_showAll);
			config_get_int_wrap(config, "normal", "tintOpacity", &ps->o.normal_tintOpacity, 0, 256);
			config_get_int_wrap(config, "normal", "opacity", &ps->o.normal_opacity, 0, 256);
			config_get_int_wrap(config, "highlight", "tintOpacity", &ps->o.highlight_tintOpacity, 0, 256);
			config_get_int_wrap(config, "highlight", "opacity", &ps->o.highlight_opacity, 0, 256);
			config_get_bool_wrap(config, "tooltip", "show", &ps->o.tooltip_show);
			config_get_int_wrap(config, "tooltip", "tintOpacity", &ps->o.highlight_tintOpacity, 0, 256);
			config_get_int_wrap(config, "tooltip", "opacity", &ps->o.tooltip_opacity, 0, 256);

			config_free(config);
		}
	}
	
	const char* pipePath = ps->o.pipePath;
	
	// Parse commandline arguments
	{
		enum options {
			OPT_ACTV_PICKER = 256,
			OPT_DM_START,
			OPT_DM_STOP,
		};
		static const char * opts_short = "hS";
		static const struct option opts_long[] = {
			{ "help",					no_argument,	NULL, 'h' },
			{ "activate-window-picker", no_argument,	NULL, OPT_ACTV_PICKER },
			{ "start-daemon",			no_argument,	NULL, OPT_DM_START },
			{ "end-daemon",				no_argument,	NULL, OPT_DM_STOP },
			{ NULL, no_argument, NULL, 0 }
		};
		int o = 0;
		while ((o = getopt_long(argc, argv, opts_short, opts_long, NULL)) >= 0) {
			switch (o) {
				case 'S': synchronize = true; break;
				case OPT_ACTV_PICKER:
					printf("activating window picker...\n");
					activate_window_picker(pipePath);
					goto main_end;
				case OPT_DM_START:
					runAsDaemon = True;
					break;
				case OPT_DM_STOP:
					printf("exiting daemon...\n");
					exit_daemon(pipePath);
					goto main_end;
				case 'h':
				default:
					show_help();
					// Return a non-zero value on unrecognized option
					if ('h' != o)
						ret = RET_BADARG;
					goto main_end;
			}
		}
	}
	
	// Open connection to X
	ps->dpy = dpy = XOpenDisplay(NULL);
	if(!dpy) {
		printfef("(): FATAL: Couldn't connect to display.");
		ret = RET_XFAIL;
		goto main_end;
	}
	if (!init_xexts(ps)) {
		ret = RET_XFAIL;
		goto main_end;
	}
	if (synchronize)
		XSynchronize(ps->dpy, True);
	XSetErrorHandler(xerror);
	
	wm_get_atoms(ps);
	
	if(! wm_check(dpy)) {
		fprintf(stderr, "FATAL: WM not NETWM or GNOME WM Spec compliant.\n");
		ret = 1;
		goto main_end;
	}
	
	wm_use_netwm_fullscreen(ps->o.useNetWMFullscreen);
	wm_ignore_skip_taskbar(ps->o.ignoreSkipTaskbar);
	
	mw = mainwin_create(ps);
	if(! mw)
	{
		fprintf(stderr, "FATAL: Couldn't create main window.\n");
		ret = 1;
		goto main_end;
	}
	
	invertShift = ps->o.xinerama_showAll;
	
	XSelectInput(mw->dpy, mw->root, PropertyChangeMask);

	if (runAsDaemon)
	{
		printf("Running as daemon...\n");

		if (access(pipePath, R_OK) == 0)
		{
			printf("access() returned 0\n");
			printf("reading excess data to end...\n");
			flush_file = 1;
		}
		
		result = mkfifo (pipePath, S_IRUSR| S_IWUSR);
		if (result < 0  && errno != EEXIST)
		{
			fprintf(stderr, "Error creating named pipe.\n");
			perror("mkfifo");
			exit(2);
		}
		
		fp = fopen(pipePath, "r");
		
		if (flush_file)
		{
			while (1)
			{
				piped_input = fgetc(fp);
				if (piped_input == EOF)
				{
					break;
				}
			}
			
			fp = fopen(pipePath, "r");
		}
		
		while (!exitDaemon)
		{
			piped_input = fgetc(fp);
			switch (piped_input)
			{
				case ACTIVATE_WINDOW_PICKER:
					leader = None, focused = wm_get_focused(mw->dpy);
					clients = skippy_run(mw, clients, focused, leader, !invertShift);
					break;
				
				case EXIT_RUNNING_DAEMON:
					printf("Exit command received, killing daemon cleanly...\n");
					remove(pipePath);
					exitDaemon = 1;
				
				case EOF:
					#ifdef DEBUG_XINERAMA
					printf("EOF reached.\n");
					#endif
					fclose(fp);
					fp = fopen(pipePath, "r");
					break;
				
				default:
					printf("unknown code received: %d\n", piped_input);
					printf("Ignoring...\n");
					break;
			}
		}
	}
	else
	{
		printf("running once then quitting...\n");
		leader = None, focused = wm_get_focused(mw->dpy);
		clients = skippy_run(mw, clients, focused, leader, !invertShift);
	}
	
	dlist_free_with_func(clients, (dlist_free_func)clientwin_destroy);
	
main_end:
	if (mw)
		mainwin_destroy(mw);
	
	// Free session data
	if (ps) {
		// Free configuration strings
		{
			free(ps->o.pipePath);
			free(ps->o.normal_tint);
			free(ps->o.highlight_tint);
			free(ps->o.tooltip_border);
			free(ps->o.tooltip_background);
			free(ps->o.tooltip_text);
			free(ps->o.tooltip_textShadow);
			free(ps->o.tooltip_font);
		}

		if (ps->dpy)
			XCloseDisplay(dpy);
		free(ps);
	}

	return ret;
}
