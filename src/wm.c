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

Atom
	/* Root pixmap / wallpaper atoms */
	_XROOTPMAP_ID,
	ESETROOT_PMAP_ID,

	// Window type atoms
	_NET_WM_WINDOW_TYPE_DESKTOP,
	_NET_WM_WINDOW_TYPE_DOCK,
	_NET_WM_WINDOW_TYPE_NORMAL,
	_NET_WM_WINDOW_TYPE_TOOLTIP;
	
static Atom
	/* Generic atoms */
	XA_WM_STATE,
	WM_CLIENT_LEADER,
	XA_UTF8_STRING,
	
	/* NetWM atoms */
	_NET_SUPPORTING_WM_CHECK,
	_NET_SUPPORTED,
	_NET_NUMBER_OF_DESKTOPS,
	_NET_CLIENT_LIST,
	_NET_CLIENT_LIST_STACKING,
	_NET_CURRENT_DESKTOP,
	_NET_WM_DESKTOP,
	_NET_WM_STATE,
	_NET_WM_STATE_HIDDEN,
	_NET_WM_STATE_SKIP_TASKBAR,
	_NET_WM_STATE_SKIP_PAGER,
	_NET_WM_STATE_FULLSCREEN,
	_NET_WM_STATE_SHADED,
	_NET_WM_STATE_ABOVE,
	_NET_WM_STATE_STICKY,
	_NET_WM_WINDOW_TYPE,
	_NET_WM_VISIBLE_NAME,
	_NET_WM_NAME,
	
	/* Old gnome atoms */
	_WIN_SUPPORTING_WM_CHECK,
	_WIN_WORKSPACE,
	_WIN_WORKSPACE_COUNT,
	_WIN_PROTOCOLS,
	_WIN_CLIENT_LIST,
	_WIN_STATE,
	_WIN_HINTS;

#define WM_PERSONALITY_NETWM 0
#define WM_PERSONALITY_GNOME 1

/* From WindowMaker's gnome.c */
#define WIN_HINTS_SKIP_FOCUS      (1<<0) /*"alt-tab" skips this win*/
#define WIN_HINTS_SKIP_WINLIST    (1<<1) /*do not show in window list*/
#define WIN_HINTS_SKIP_TASKBAR    (1<<2) /*do not show on taskbar*/
#define WIN_HINTS_GROUP_TRANSIENT (1<<3) /*Reserved - definition is unclear*/
#define WIN_HINTS_FOCUS_ON_CLICK  (1<<4) /*app only accepts focus if clicked*/
#define WIN_HINTS_DO_NOT_COVER    (1<<5) /* attempt to not cover this window */


#define WIN_STATE_STICKY          (1<<0) /*everyone knows sticky*/
#define WIN_STATE_MINIMIZED       (1<<1) /*Reserved - definition is unclear*/
#define WIN_STATE_MAXIMIZED_VERT  (1<<2) /*window in maximized V state*/
#define WIN_STATE_MAXIMIZED_HORIZ (1<<3) /*window in maximized H state*/
#define WIN_STATE_HIDDEN          (1<<4) /*not on taskbar but window visible*/
#define WIN_STATE_SHADED          (1<<5) /*shaded (MacOS / Afterstep style)*/
/* these are bogus states defined in "the spec" */
#define WIN_STATE_HID_WORKSPACE   (1<<6) /*not on current desktop*/
#define WIN_STATE_HID_TRANSIENT   (1<<7) /*owner of transient is hidden*/
#define WIN_STATE_FIXED_POSITION  (1<<8) /*window is fixed in position even*/
#define WIN_STATE_ARRANGE_IGNORE  (1<<9) /*ignore for auto arranging*/


static int WM_PERSONALITY = WM_PERSONALITY_NETWM,
           NETWM_HAS_FULLSCREEN = 0,
           IGNORE_SKIP_TASKBAR = 0;

/**
 * @brief Wrapper of XInternAtom().
 */
static inline Atom
get_atom(session_t *ps, const char *name) {
	return XInternAtom(ps->dpy, name, False);
}

/**
 * @brief Initialize X atoms.
 */
void
wm_get_atoms(session_t *ps) {
#define T_GETATOM(name) name = get_atom(ps, # name)
	XA_WM_STATE = get_atom(ps, "WM_STATE");
	T_GETATOM(WM_CLIENT_LEADER);
	XA_UTF8_STRING = get_atom(ps, "UTF8_STRING");
	
	T_GETATOM(_XROOTPMAP_ID);
	T_GETATOM(ESETROOT_PMAP_ID);
	
	T_GETATOM(_NET_SUPPORTING_WM_CHECK);
	T_GETATOM(_NET_SUPPORTED);
	T_GETATOM(_NET_NUMBER_OF_DESKTOPS);
	T_GETATOM(_NET_CLIENT_LIST);
	T_GETATOM(_NET_CLIENT_LIST_STACKING);
	T_GETATOM(_NET_CURRENT_DESKTOP);
	T_GETATOM(_NET_WM_DESKTOP);
	T_GETATOM(_NET_WM_STATE);
	T_GETATOM(_NET_WM_STATE_HIDDEN);
	T_GETATOM(_NET_WM_STATE_SKIP_TASKBAR);
	T_GETATOM(_NET_WM_STATE_SKIP_PAGER);
	T_GETATOM(_NET_WM_STATE_FULLSCREEN);
	T_GETATOM(_NET_WM_STATE_SHADED);
	T_GETATOM(_NET_WM_STATE_ABOVE);
	T_GETATOM(_NET_WM_STATE_STICKY);
	T_GETATOM(_NET_WM_WINDOW_TYPE);
	T_GETATOM(_NET_WM_WINDOW_TYPE_DESKTOP);
	T_GETATOM(_NET_WM_WINDOW_TYPE_DOCK);
	T_GETATOM(_NET_WM_WINDOW_TYPE_NORMAL);
	T_GETATOM(_NET_WM_WINDOW_TYPE_TOOLTIP);
	T_GETATOM(_NET_WM_VISIBLE_NAME);
	T_GETATOM(_NET_WM_NAME);
	
	T_GETATOM(_WIN_SUPPORTING_WM_CHECK);
	T_GETATOM(_WIN_WORKSPACE);
	T_GETATOM(_WIN_WORKSPACE_COUNT);
	T_GETATOM(_WIN_PROTOCOLS);
	T_GETATOM(_WIN_CLIENT_LIST);
	T_GETATOM(_WIN_STATE);
	T_GETATOM(_WIN_HINTS);
#undef T_GETATOM
}

char
wm_check_netwm(Display *dpy)
{
	Window wm_check;
	unsigned char *data, *data2;
	
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left, i;
	
	char req = 0;
	
	status = XGetWindowProperty(dpy, DefaultRootWindow(dpy), _NET_SUPPORTING_WM_CHECK,
	                  0L, 1L, False, XA_WINDOW, &real_type, &real_format,
	                  &items_read, &items_left, &data);
	if(status != Success || ! items_read) {
		if(status == Success)
			XFree(data);
		return 0;
	}
	
	wm_check = ((Window*)data)[0];
	XFree(data);
	
	status = XGetWindowProperty(dpy, wm_check, _NET_SUPPORTING_WM_CHECK,
	                  0L, 1L, False, XA_WINDOW, &real_type, &real_format,
	                  &items_read, &items_left, &data);
	
	if(status != Success && ! items_read) {
		if(status == Success)
			XFree(data);
		return 0;
	}
	
	if(wm_check != ((Window*)data)[0]) {
		XFree(data);
		return 0;
	}
	
	XFree(data);
	
	status = XGetWindowProperty(dpy, DefaultRootWindow(dpy), _NET_SUPPORTED,
	                  0L, 8192L, False, XA_ATOM, &real_type, &real_format,
	                  &items_read, &items_left, &data2);
	
	if(status != Success || ! items_read) {
		if(status == Success)
			XFree(data2);
		return 0;
	}
	
	for(i = 0; i < items_read; i++) {
		if(((Atom*)data2)[i] == _NET_NUMBER_OF_DESKTOPS)
			req |= 1;
		else if(((Atom*)data2)[i] == _NET_CURRENT_DESKTOP)
			req |= 2;
		else if(((Atom*)data2)[i] == _NET_WM_STATE)
			req |= 4;
		else if(((Atom*)data2)[i] == _NET_CLIENT_LIST)
			req |= 8;
		else if(((Atom*)data2)[i] == _NET_CLIENT_LIST_STACKING)
			req |= 16;
		else if(((Atom*)data2)[i] == _NET_WM_STATE_FULLSCREEN)
			NETWM_HAS_FULLSCREEN = 1;
	}
	XFree(data2);
	if(req & 16) {
		req |= 8;
		_NET_CLIENT_LIST = _NET_CLIENT_LIST_STACKING;
	} 
	
	return ((req & 15) == 15);
}

char
wm_check_gnome(Display *dpy)
{
	Window wm_check;
	unsigned char *data, *data2;
	
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left, i;
	
	char req = 0;
	
	WM_PERSONALITY = WM_PERSONALITY_GNOME;
	
	status = XGetWindowProperty(dpy, DefaultRootWindow(dpy), _WIN_SUPPORTING_WM_CHECK,
	                  0L, 1L, False, XA_CARDINAL, &real_type, &real_format,
	                  &items_read, &items_left, &data);
	if(status != Success || ! items_read) {
		if(status == Success)
			XFree(data);
		return 0;
	}
	
	wm_check = ((Window*)data)[0];
	XFree(data);
	
	status = XGetWindowProperty(dpy, wm_check, _WIN_SUPPORTING_WM_CHECK,
	                  0L, 1L, False, XA_CARDINAL, &real_type, &real_format,
	                  &items_read, &items_left, &data);
	
	if(status != Success && ! items_read) {
		if(status == Success)
			XFree(data);
		return 0;
	}
	
	if(wm_check != ((Window*)data)[0]) {
		XFree(data);
		return 0;
	}
	
	XFree(data);
	
	status = XGetWindowProperty(dpy, DefaultRootWindow(dpy), _WIN_PROTOCOLS,
	                  0L, 8192L, False, XA_ATOM, &real_type, &real_format,
	                  &items_read, &items_left, &data2);
	
	if(status != Success || ! items_read) {
		if(status == Success)
			XFree(data2);
		return 0;
	}
	
	for(i = 0; i < items_read; i++) {
		if(((Atom*)data2)[i] == _WIN_WORKSPACE)
			req |= 1;
		else if(((Atom*)data2)[i] == _WIN_WORKSPACE_COUNT)
			req |= 2;
		else if(((Atom*)data2)[i] == _WIN_STATE)
			req |= 4;
		else if(((Atom*)data2)[i] == _WIN_CLIENT_LIST)
			req |= 8;
	}
	XFree(data2);
	
	return ((req & 15) == 15);
}

char
wm_check(Display *dpy)
{
	return wm_check_netwm(dpy) || wm_check_gnome(dpy);
}

dlist *
wm_get_stack(Display *dpy)
{
	dlist *l = 0;
	unsigned char *data;
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left, i;
	
	if(WM_PERSONALITY == WM_PERSONALITY_NETWM)
		status = XGetWindowProperty(dpy, DefaultRootWindow(dpy), _NET_CLIENT_LIST,
		                  0L, 8192L, False, XA_WINDOW, &real_type, &real_format,
		                  &items_read, &items_left, &data);
	else
		status = XGetWindowProperty(dpy, DefaultRootWindow(dpy), _WIN_CLIENT_LIST,
		                  0L, 8192L, False, XA_CARDINAL, &real_type, &real_format,
		                  &items_read, &items_left, &data);
	
	if(status != Success)
		return 0;
	
	for(i = 0; i < items_read; i++)
	{
		l = dlist_add(l, (void *)((long *)data)[i]);
	}
	
	XFree(data);
	
	return l;
}

Pixmap
wm_get_root_pmap(Display *dpy)
{
	Pixmap rootpmap = None;
	unsigned char *data;
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left;
	
	status = XGetWindowProperty(dpy, DefaultRootWindow(dpy), _XROOTPMAP_ID,
	                  0L, 1L, False, XA_PIXMAP, &real_type, &real_format,
	                  &items_read, &items_left, &data);
	if(status != Success) {
		status = XGetWindowProperty(dpy, DefaultRootWindow(dpy), ESETROOT_PMAP_ID,
		                  0L, 1L, False, XA_PIXMAP, &real_type, &real_format,
		                  &items_read, &items_left, &data);
		if(status != Success)
			return None;
	}
	
	if(items_read)
		rootpmap = ((Pixmap*)data)[0];
	
	XFree(data);
	
	return rootpmap;
}

CARD32
wm_get_current_desktop(Display *dpy)
{
	CARD32 desktop = 0;
	unsigned char *data;
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left;
	
	status = XGetWindowProperty(dpy, DefaultRootWindow(dpy),
	                  (WM_PERSONALITY == WM_PERSONALITY_NETWM) ?  _NET_CURRENT_DESKTOP : _WIN_WORKSPACE,
	                  0L, 1L, False, XA_CARDINAL, &real_type, &real_format,
	                  &items_read, &items_left, &data);
	if(status != Success)
		return 0;
	if(items_read)
		desktop = ((CARD32*)data)[0];
	XFree(data);
	
	return desktop;
}

FcChar8 *
wm_get_window_title(Display *dpy, Window window, int *length_return)
{
	unsigned char *data;
	FcChar8 *ret = 0;
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left;
	
	*length_return = 0;
	
	status = XGetWindowProperty(dpy, window, _NET_WM_VISIBLE_NAME,
	                  0, 8192, False, XA_UTF8_STRING, &real_type, &real_format,
	                  &items_read, &items_left, &data);
	if(status != Success || items_read == 0)
	{
		if(status == Success)
			XFree(data);
		status = XGetWindowProperty(dpy, window, _NET_WM_NAME,
		                  0, 8192, False, XA_UTF8_STRING, &real_type, &real_format,
		                  &items_read, &items_left, &data);
	}
	if(status != Success || items_read == 0)
	{
		if(status == Success)
			XFree(data);
		status = XGetWindowProperty(dpy, window, XA_WM_NAME,
		                  0, 8192, False, XA_STRING, &real_type, &real_format,
		                  &items_read, &items_left, &data);
	}
	if(status != Success)
		return 0;
	
	if(items_read)
	{
		ret = (FcChar8 *)malloc(items_read);
		memcpy(ret, data, items_read);
		*length_return = items_read;
	}
	
	XFree(data);
	
	return ret;
}

Window
wm_get_group_leader(Display *dpy, Window window)
{
	unsigned char *data;
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left;
	Window leader = None;
	
	status = XGetWindowProperty(dpy, window, WM_CLIENT_LEADER,
	                  0, 1, False, XA_WINDOW, &real_type, &real_format,
	                  &items_read, &items_left, &data);
	
	if(status != Success)
	{
		XWMHints *hints = XGetWMHints(dpy, window);
		if(! hints)
			return None;
		
		if(hints->flags & WindowGroupHint)
			leader = hints->window_group;
		
		return leader;
	}
	
	if(items_read)
		leader = ((Window*)data)[0];
	
	XFree(data);
	
	return leader;
}

void
wm_use_netwm_fullscreen(Bool b)
{
	NETWM_HAS_FULLSCREEN = b ? NETWM_HAS_FULLSCREEN : False;
}

void
wm_ignore_skip_taskbar(Bool b)
{
	IGNORE_SKIP_TASKBAR = b;
}

void
wm_set_fullscreen(Display *dpy, Window window, int x, int y, unsigned int width, unsigned int height)
{
	if(WM_PERSONALITY == WM_PERSONALITY_NETWM && NETWM_HAS_FULLSCREEN)
	{
		Atom props[6];
		CARD32 desktop = (CARD32)-1;
		
		props[0] = _NET_WM_STATE_FULLSCREEN;
		props[1] = _NET_WM_STATE_SKIP_TASKBAR;
		props[2] = _NET_WM_STATE_SKIP_PAGER;
		props[3] = _NET_WM_STATE_ABOVE;
		props[4] = _NET_WM_STATE_STICKY;
		props[5] = 0;
		XChangeProperty(dpy, window, _NET_WM_STATE, XA_ATOM, 32, PropModeReplace, (unsigned char*)props, 5);
		XChangeProperty(dpy, window, _NET_WM_DESKTOP, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&desktop, 1);
	}
	else
	{
		XSetWindowAttributes wattr;
		wattr.override_redirect = True;
		XChangeWindowAttributes(dpy, window, CWOverrideRedirect, &wattr);
		XMoveResizeWindow(dpy, window, x, y, width, height);
	}
}

int
wm_validate_window(Display *dpy, Window win)
{
	unsigned char *data;
	Atom *atoms;
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left, i;
	int result = 1;
	
	if(WM_PERSONALITY == WM_PERSONALITY_NETWM)
	{
		status = XGetWindowProperty(dpy, win, _NET_WM_STATE,
		                  0L, 8192L, False, XA_ATOM, &real_type, &real_format,
		                  &items_read, &items_left, &data);
		
		if(status != Success)
			return 0;
		
		atoms = (Atom *)data;
		
		for(i = 0; result && i < items_read; i++) {
			if(atoms[i] == _NET_WM_STATE_HIDDEN)
				result = 0;
			else if(! IGNORE_SKIP_TASKBAR && atoms[i] == _NET_WM_STATE_SKIP_TASKBAR)
				result = 0;
			else if(atoms[i] == _NET_WM_STATE_SHADED)
				result = 0;
			if(! result)
				break;
		}
		XFree(data);
		
		if(! result)
			return 0;
		
		status = XGetWindowProperty(dpy, win, _NET_WM_WINDOW_TYPE,
		                            0L, 1L, False, XA_ATOM, &real_type, &real_format,
		                            &items_read, &items_left, &data);
		if(status != Success)
			return 1;
		
		atoms = (Atom *)data;
		
		if(items_read && (atoms[0] == _NET_WM_WINDOW_TYPE_DESKTOP || atoms[0] == _NET_WM_WINDOW_TYPE_DOCK))
			result = 0;
		
		XFree(data);
		
		return result;
	} else {
		CARD32 attr;
		
		status = XGetWindowProperty(dpy, win, _WIN_STATE,
		                  0L, 1L, False, XA_CARDINAL, &real_type, &real_format,
		                  &items_read, &items_left, &data);
		if(status != Success || ! items_read)
		{
			if(status == Success)
				XFree(data);
			return 0;
		}
		attr = (((CARD32*)data)[0]) & (WIN_STATE_MINIMIZED |
		                             WIN_STATE_SHADED |
		                             WIN_STATE_HIDDEN);
		if(attr)
			result = 0;
		XFree(data);
		if(! result)
			return 0;
		
		if(! IGNORE_SKIP_TASKBAR)
		{
			status = XGetWindowProperty(dpy, win, _WIN_HINTS,
			                  0L, 1L, False, XA_CARDINAL, &real_type, &real_format,
			                  &items_read, &items_left, &data);
			if(status != Success || ! items_read)
			{
				if(status == Success)
					XFree(data);
				return 1; /* If there's no _WIN_HINTS, assume it's 0, thus valid */
			}
			attr = ((CARD32*)data)[0];
			if(attr & WIN_HINTS_SKIP_TASKBAR)
				result = 0;
			XFree(data);
		}
		
		return result;
	}
}

CARD32
wm_get_window_desktop(Display *dpy, Window win)
{
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left;
	unsigned char *data;
	CARD32 desktop = 0;
	
	if(WM_PERSONALITY == WM_PERSONALITY_GNOME)
	{
		status = XGetWindowProperty(dpy, win, _WIN_STATE,
		                  0L, 1L, False, XA_CARDINAL, &real_type, &real_format,
		                  &items_read, &items_left, &data);
		if(status == Success)
		{
			if(items_read)
				desktop = (((CARD32*)data)[0] & WIN_STATE_STICKY) ? (CARD32)-1 : 0;
			
			XFree(data);
			
			if(desktop)
				return desktop;
		}
	}
	
	status = XGetWindowProperty(dpy, win,
	                  (WM_PERSONALITY == WM_PERSONALITY_NETWM) ? _NET_WM_DESKTOP : _WIN_WORKSPACE,
	                  0L, 1L, False, XA_CARDINAL, &real_type, &real_format,
	                  &items_read, &items_left, &data);
	
	if(status != Success)
		return wm_get_current_desktop(dpy);
	
	if(items_read)
		desktop = ((CARD32*)data)[0];
	else
		desktop = wm_get_current_desktop(dpy);
	
	XFree(data);
	
	return desktop;
}

/* Get focused window and traverse towards the root window until a window with WM_STATE is found */
Window
wm_get_focused(Display *dpy)
{
	Window focused = None, root = None, *children;
	unsigned int tmp_u;
	int revert_to, status, real_format;
	Atom real_type;
	unsigned long items_read, items_left;
	unsigned char *data;
	
	XGetInputFocus(dpy, &focused, &revert_to);
	
	while(focused != None && focused != root)
	{
		status = XGetWindowProperty(dpy, focused, XA_WM_STATE,
		                            0L, 1L, False, XA_WM_STATE, &real_type, &real_format,
		                            &items_read, &items_left, &data);
		if(status == Success)
		{
			XFree(data);
			if(items_read)
				break;
		}
		XQueryTree(dpy, focused, &root, &focused, &children, &tmp_u);
		if(children)
			XFree(children);
	}
	
	return focused;
}

/**
 * @brief Set a UTF-8 string property on a window.
 */
bool
wm_wid_set_prop_utf8(session_t *ps, Window wid, Atom prop, char *text) {
	XTextProperty text_prop = { };
	bool success = (Success == XmbTextListToTextProperty(ps->dpy, &text, 1,
				XUTF8StringStyle, &text_prop));
	if (success)
		XSetTextProperty(ps->dpy, wid, &text_prop, prop);
	sxfree(text_prop.value);
	return success;
}

/**
 * @brief Set basic properties on a window.
 */
void
wm_wid_set_info(session_t *ps, Window wid, const char *name,
		Atom window_type) {
	// Set window name
	{
		char *textcpy = mstrjoin("skippy-xd ", name);
		{
			XTextProperty text_prop = { };
			if (Success == XmbTextListToTextProperty(ps->dpy, &textcpy, 1,
						XStdICCTextStyle, &text_prop))
				XSetWMName(ps->dpy, wid, &text_prop);
			sxfree(text_prop.value);
		}
		wm_wid_set_prop_utf8(ps, wid, _NET_WM_NAME, textcpy);
		free(textcpy);
	}

	// Set window class
	{
		XClassHint *classh = allocchk(XAllocClassHint());
		classh->res_name = "skippy-xd";
		classh->res_class = "skippy-xd";
		XSetClassHint(ps->dpy, wid, classh);
		XFree(classh);
	}

	// Set window type
	{
		if (!window_type)
			window_type = _NET_WM_WINDOW_TYPE_NORMAL;
		long val = window_type;
		XChangeProperty(ps->dpy, wid, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
				PropModeReplace, (unsigned char *) &val, 1);
	}
}
