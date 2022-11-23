/*
 * tkMacOSXWindowEvent.c --
 *
 *	This file defines the routines for both creating and handling Window
 *	Manager class events for Tk.
 *
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2015 Kevin Walzer/WordTech Communications LLC.
 * Copyright (c) 2015 Marc Culler.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXWm.h"
#include "tkMacOSXEvent.h"
#include "tkMacOSXDebug.h"
#include "tkMacOSXConstants.h"

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_EVENTS
#define TK_MAC_DEBUG_DRAWING
#endif
*/

/*
 * Declaration of functions used only in this file
 */

static int		GenerateUpdates(
			    CGRect *updateBounds, TkWindow *winPtr);
static int		GenerateActivateEvents(TkWindow *winPtr,
			    int activeFlag);

#pragma mark TKApplication(TKWindowEvent)

extern NSString *NSWindowDidOrderOnScreenNotification;
extern NSString *NSWindowWillOrderOnScreenNotification;

#ifdef TK_MAC_DEBUG_NOTIFICATIONS
extern NSString *NSWindowDidOrderOffScreenNotification;
#endif


@implementation TKApplication(TKWindowEvent)

- (void) windowActivation: (NSNotification *) notification
{
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    BOOL activate = [[notification name]
	    isEqualToString:NSWindowDidBecomeKeyNotification];
    NSWindow *w = [notification object];
    TkWindow *winPtr = TkMacOSXGetTkWindow(w);

    if (winPtr && Tk_IsMapped(winPtr)) {
	GenerateActivateEvents(winPtr, activate);
    }
}

- (void) windowBoundsChanged: (NSNotification *) notification
{
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    BOOL movedOnly = [[notification name]
	    isEqualToString:NSWindowDidMoveNotification];
    NSWindow *w = [notification object];
    TkWindow *winPtr = TkMacOSXGetTkWindow(w);

    if (winPtr) {
	WmInfo *wmPtr = winPtr->wmInfoPtr;
	NSRect bounds = [w frame];
	int x, y, width = -1, height = -1, flags = 0;

	x = bounds.origin.x;
	y = TkMacOSXZeroScreenHeight() - (bounds.origin.y + bounds.size.height);
	if (winPtr->changes.x != x || winPtr->changes.y != y) {
	    flags |= TK_LOCATION_CHANGED;
	} else {
	    x = y = -1;
	}
	if (!movedOnly && (winPtr->changes.width != bounds.size.width ||
		winPtr->changes.height !=  bounds.size.height)) {
	    width = bounds.size.width - wmPtr->xInParent;
	    height = bounds.size.height - wmPtr->yInParent;
	    flags |= TK_SIZE_CHANGED;
	}
	/*
	 * Propagate geometry changes immediately.
	 */

	flags |= TK_MACOSX_HANDLE_EVENT_IMMEDIATELY;
	TkGenWMConfigureEvent((Tk_Window)winPtr, x, y, width, height, flags);
    }

}

- (void) windowExpanded: (NSNotification *) notification
{
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    NSWindow *w = [notification object];
    TkWindow *winPtr = TkMacOSXGetTkWindow(w);

    if (winPtr) {
	winPtr->wmInfoPtr->hints.initial_state =
		TkMacOSXIsWindowZoomed(winPtr) ? ZoomState : NormalState;
	Tk_MapWindow((Tk_Window)winPtr);

	/*
	 * Process all Tk events generated by Tk_MapWindow().
	 */

	while (Tcl_ServiceEvent(0)) {}
	while (Tcl_DoOneEvent(TCL_IDLE_EVENTS)) {}

	/*
	 * NSWindowDidDeminiaturizeNotification is received after
	 * NSWindowDidBecomeKeyNotification, so activate manually
	 */

	GenerateActivateEvents(winPtr, 1);
    }
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)newFrame
{
    (void)window;

    /*
     * This method needs to be implemented in order for [NSWindow isZoomed] to
     * give the correct answer. But it suffices to always validate every
     * request.
     */

    return newFrame;
}

- (NSSize)window:(NSWindow *)window
  willUseFullScreenContentSize:(NSSize)proposedSize
{
    (void)window;

    /*
     * We don't need to change the proposed size, but we do need to implement
     * this method.  Otherwise the full screen window will be sized to the
     * screen's visibleFrame, leaving black bands at the top and bottom.
     */

    return proposedSize;
}

- (void) windowEnteredFullScreen: (NSNotification *) notification
{
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    if (![[notification object] respondsToSelector: @selector (tkLayoutChanged)]) {
	return;
    }
    [(TKWindow *)[notification object] tkLayoutChanged];
}

- (void) windowExitedFullScreen: (NSNotification *) notification
{
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    if (![[notification object] respondsToSelector: @selector (tkLayoutChanged)]) {
	return;
    }
    [(TKWindow *)[notification object] tkLayoutChanged];
}

- (void) windowCollapsed: (NSNotification *) notification
{
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    NSWindow *w = [notification object];
    TkWindow *winPtr = TkMacOSXGetTkWindow(w);

    if (winPtr) {
	Tk_UnmapWindow((Tk_Window)winPtr);
    }
}

- (BOOL) windowShouldClose: (NSWindow *) w
{
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, w);
#endif
    TkWindow *winPtr = TkMacOSXGetTkWindow(w);

    if (winPtr) {
	TkGenWMDestroyEvent((Tk_Window)winPtr);
    }

    /*
     * If necessary, TkGenWMDestroyEvent() handles [close]ing the window, so
     * can always return NO from -windowShouldClose: for a Tk window.
     */

    return (winPtr ? NO : YES);
}

- (void) windowBecameVisible: (NSNotification *) notification
{
    NSWindow *window = [notification object];
    TkWindow *winPtr = TkMacOSXGetTkWindow(window);
    if (winPtr) {
	TKContentView *view = [window contentView];

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
	if (@available(macOS 10.15, *)) {
	    [view viewDidChangeEffectiveAppearance];
	}
#endif
	[view addTkDirtyRect:[view bounds]];
	Tcl_CancelIdleCall(TkMacOSXDrawAllViews, NULL);
	Tcl_DoWhenIdle(TkMacOSXDrawAllViews, NULL);
    }
}

- (void) windowMapped: (NSNotification *) notification
{
    NSWindow *w = [notification object];
    TkWindow *winPtr = TkMacOSXGetTkWindow(w);

    if (winPtr) {
	while (Tcl_DoOneEvent(TCL_IDLE_EVENTS)) {}
    }
}

#ifdef TK_MAC_DEBUG_NOTIFICATIONS

- (void) windowDragStart: (NSNotification *) notification
{
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
}

- (void) windowLiveResize: (NSNotification *) notification
{
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
    //BOOL start = [[notification name] isEqualToString:NSWindowWillStartLiveResizeNotification];
}

- (void) windowUnmapped: (NSNotification *) notification
{
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
    NSWindow *w = [notification object];
    TkWindow *winPtr = TkMacOSXGetTkWindow(w);

    if (winPtr) {
	//Tk_UnmapWindow((Tk_Window)winPtr);
    }
}

#endif /* TK_MAC_DEBUG_NOTIFICATIONS */

- (void) _setupWindowNotifications
{
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];

#define observe(n, s) \
	[nc addObserver:self selector:@selector(s) name:(n) object:nil]

    observe(NSWindowDidBecomeKeyNotification, windowActivation:);
    observe(NSWindowDidResignKeyNotification, windowActivation:);
    observe(NSWindowDidMoveNotification, windowBoundsChanged:);
    observe(NSWindowDidResizeNotification, windowBoundsChanged:);
    observe(NSWindowDidDeminiaturizeNotification, windowExpanded:);
    observe(NSWindowDidMiniaturizeNotification, windowCollapsed:);
    observe(NSWindowWillOrderOnScreenNotification, windowMapped:);
    observe(NSWindowDidOrderOnScreenNotification, windowBecameVisible:);

#if !(MAC_OS_X_VERSION_MAX_ALLOWED < 1070)
    observe(NSWindowDidEnterFullScreenNotification, windowEnteredFullScreen:);
    observe(NSWindowDidExitFullScreenNotification, windowExitedFullScreen:);
#endif

#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    observe(NSWindowWillMoveNotification, windowDragStart:);
    observe(NSWindowWillStartLiveResizeNotification, windowLiveResize:);
    observe(NSWindowDidEndLiveResizeNotification, windowLiveResize:);
    observe(NSWindowDidOrderOffScreenNotification, windowUnmapped:);
#endif
#undef observe

}
@end


/*
 * Idle task which forces focus to a particular window.
 */

static void RefocusGrabWindow(void *data) {
    TkWindow *winPtr = (TkWindow *) data;
    TkpChangeFocus(winPtr, 1);
}

#pragma mark TKApplication(TKApplicationEvent)

@implementation TKApplication(TKApplicationEvent)

- (void) applicationActivate: (NSNotification *) notification
{
    (void)notification;

#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    [NSApp tkCheckPasteboard];

    /*
     * When the application is activated with Command-Tab it will create a
     * zombie window for every Tk window which has been withdrawn.  So iterate
     * through the list of windows and order out any withdrawn window.
     * If one of the windows is the grab window for its display we focus
     * it.  This is done as at idle, in case the app was reactivated by
     * clicking a different window.  In that case we need to wait until the
     * mouse event has been processed before focusing the grab window.
     */

    for (NSWindow *win in [NSApp windows]) {
	TkWindow *winPtr = TkMacOSXGetTkWindow(win);
	if (!winPtr || !winPtr->wmInfoPtr) {
	    continue;
	}
	if (winPtr->wmInfoPtr->hints.initial_state == WithdrawnState) {
	    [win orderOut:nil];
	}
	if (winPtr->dispPtr->grabWinPtr == winPtr) {
	    Tcl_DoWhenIdle(RefocusGrabWindow, winPtr);
	} else {
	    [[self keyWindow] orderFront: self];
	}
    }
}

- (void) applicationDeactivate: (NSNotification *) notification
{
    (void)notification;

#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif

    /*
     * To prevent zombie windows on systems with a TouchBar, set the key window
     * to nil if the current key window is not visible.  This allows a closed
     * Help or About window to be deallocated so it will not reappear as a
     * zombie when the app is reactivated.
     */

    NSWindow *keywindow = [NSApp keyWindow];
    if (keywindow && ![keywindow isVisible]) {
	[NSApp _setKeyWindow:nil];
	[NSApp _setMainWindow:nil];
    }

}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender
                    hasVisibleWindows:(BOOL)flag
{
    (void)sender;
    (void)flag;

    /*
     * Allowing the default response means that withdrawn windows will get
     * displayed on the screen with unresponsive title buttons.  We don't
     * really want that.  Besides, we can write our own code to handle this
     * with ::tk::mac::ReopenApplication.  So we just say NO.
     */

    return NO;
}


- (void) applicationShowHide: (NSNotification *) notification
{
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    const char *cmd = ([[notification name] isEqualToString:
	    NSApplicationDidUnhideNotification] ?
	    "::tk::mac::OnShow" : "::tk::mac::OnHide");

    if (_eventInterp && Tcl_FindCommand(_eventInterp, cmd, NULL, 0)) {
	int code = Tcl_EvalEx(_eventInterp, cmd, -1, TCL_EVAL_GLOBAL);

	if (code != TCL_OK) {
	    Tcl_BackgroundException(_eventInterp, code);
	}
	Tcl_ResetResult(_eventInterp);
    }
}

- (void) displayChanged: (NSNotification *) notification
{
    (void)notification;

#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    TkDisplay *dispPtr = TkGetDisplayList();

    if (dispPtr) {
	TkMacOSXDisplayChanged(dispPtr->display);
    }
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * TkpWillDrawWidget --
 *
 *      A widget display procedure can call this to determine whether it is
 *      being run inside of the drawRect method. If not, it may be desirable
 *      for the display procedure to simply clear the REDRAW_PENDING flag
 *      and return.  The widget can be recorded in order to schedule a
 *      redraw, via and Expose event, from within drawRect.
 *
 *      This is also needed for some tests, especially of the Text widget,
 *      which record data in a global Tcl variable and assume that display
 *      procedures will be run in a predictable sequence as Tcl idle tasks.
 *
 * Results:
 *      True if called from the drawRect method of a TKContentView with
 *      tkwin NULL or pointing to a widget in the current focusView.
 *
 * Side effects:
 *	Currently none.  One day the tkwin parameter may be recorded to
 *      handle redrawing the widget later.
 *
 *----------------------------------------------------------------------
 */

int
TkpWillDrawWidget(Tk_Window tkwin) {
    int result;
    if (tkwin) {
	TkWindow *winPtr = (TkWindow *)tkwin;
	TKContentView *view = (TKContentView *)TkMacOSXGetNSViewForDrawable(
	    (Drawable)winPtr->privatePtr);
	result = ([NSApp isDrawing] && view == [NSView focusView]);
#if 0
	printf("TkpWillDrawWidget: %s %d  %d \n", Tk_PathName(tkwin),
	       [NSApp isDrawing], (view == [NSView focusView]));
	if (!result) {
	    NSRect dirtyRect;
	    TkMacOSXWinNSBounds(winPtr, view, &dirtyRect);
	    printf("TkpAppCanDraw: dirtyRect for %s is %s\n",
		   Tk_PathName(tkwin),
		   NSStringFromRect(dirtyRect).UTF8String);
	    [view addTkDirtyRect:dirtyRect];
	}
#endif
    } else {
	result = [NSApp isDrawing];
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateUpdates --
 *
 *	Given an update rectangle and a Tk window, this function generates
 *	an X Expose event for the window if it meets the update region. The
 *	function will then recursively have each damaged window generate Expose
 *	events for its child windows.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be placed on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateUpdates(
    CGRect *updateBounds,
    TkWindow *winPtr)
{
    TkWindow *childPtr;
    XEvent event;
    CGRect bounds, damageBounds;

    TkMacOSXWinCGBounds(winPtr, &bounds);
    if (!CGRectIntersectsRect(bounds, *updateBounds)) {
	return 0;
    }

    /*
     * Compute the bounding box of the area that the damage occurred in.
     */

    damageBounds = CGRectIntersection(bounds, *updateBounds);
    event.xany.serial = LastKnownRequestProcessed(Tk_Display(winPtr));
    event.xany.send_event = false;
    event.xany.window = Tk_WindowId(winPtr);
    event.xany.display = Tk_Display(winPtr);
    event.type = Expose;
    event.xexpose.x = damageBounds.origin.x - bounds.origin.x;
    event.xexpose.y = damageBounds.origin.y - bounds.origin.y;
    event.xexpose.width = damageBounds.size.width;
    event.xexpose.height = damageBounds.size.height;
    event.xexpose.count = 0;
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);

#ifdef TK_MAC_DEBUG_DRAWING
    TKLog(@"Exposed %p {{%d, %d}, {%d, %d}}", event.xany.window, event.xexpose.x,
	event.xexpose.y, event.xexpose.width, event.xexpose.height);
#endif

    /*
     * Generate updates for the children of this window
     */

    for (childPtr = winPtr->childList; childPtr != NULL;
	    childPtr = childPtr->nextPtr) {
	if (!Tk_IsMapped(childPtr) || Tk_IsTopLevel(childPtr)) {
	    continue;
	}
	GenerateUpdates(updateBounds, childPtr);
    }

    /*
     * Generate updates for any contained windows
     */

    if (Tk_IsContainer(winPtr)) {
	childPtr = TkpGetOtherWindow(winPtr);
	if (childPtr != NULL && Tk_IsMapped(childPtr)) {
	    GenerateUpdates(updateBounds, childPtr);
	}

	/*
	 * TODO: Here we should handle out of process embedding.
	 */
    }

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGenerateFocusEvent --
 *
 *	Given a Macintosh window activate event this function generates all
 *	the X Focus events needed by Tk.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be placed on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

static int
TkMacOSXGenerateFocusEvent(
    TkWindow *winPtr,		/* Root X window for event. */
    int activeFlag)
{
    XEvent event;

    /*
     * Don't send focus events to windows of class help or to windows with the
     * kWindowNoActivatesAttribute.
     */

    if (winPtr->wmInfoPtr && (winPtr->wmInfoPtr->macClass == kHelpWindowClass ||
	    winPtr->wmInfoPtr->attributes & kWindowNoActivatesAttribute)) {
	return false;
    }

    /*
     * Generate FocusIn and FocusOut events. This event is only sent to the
     * toplevel window.
     */

    if (activeFlag) {
	event.xany.type = FocusIn;
    } else {
	event.xany.type = FocusOut;
    }

    event.xany.serial = LastKnownRequestProcessed(Tk_Display(winPtr));
    event.xany.send_event = False;
    event.xfocus.display = Tk_Display(winPtr);
    event.xfocus.window = winPtr->window;
    event.xfocus.mode = NotifyNormal;
    event.xfocus.detail = NotifyDetailNone;

    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    return true;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateActivateEvents --
 *
 *	Given a Macintosh window activate event this function generates all the
 *	X Activate events needed by Tk.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be placed on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

int
GenerateActivateEvents(
    TkWindow *winPtr,
    int activeFlag)
{
    TkGenerateActivateEvents(winPtr, activeFlag);
    if (activeFlag || ![NSApp isActive]) {
	TkMacOSXGenerateFocusEvent(winPtr, activeFlag);
    }
    return true;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGenWMConfigureEvent --
 *
 *	Generate a ConfigureNotify event for Tk. Depending on the value of flag
 *	the values of width/height, x/y, or both may be changed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A ConfigureNotify event is sent to Tk.
 *
 *----------------------------------------------------------------------
 */

void
TkGenWMConfigureEvent(
    Tk_Window tkwin,
    int x, int y,
    int width, int height,
    int flags)
{
    XEvent event;
    WmInfo *wmPtr;
    TkWindow *winPtr = (TkWindow *) tkwin;

    if (tkwin == NULL) {
	return;
    }

    event.type = ConfigureNotify;
    event.xconfigure.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
    event.xconfigure.send_event = False;
    event.xconfigure.display = Tk_Display(tkwin);
    event.xconfigure.event = Tk_WindowId(tkwin);
    event.xconfigure.window = Tk_WindowId(tkwin);
    event.xconfigure.border_width = winPtr->changes.border_width;
    event.xconfigure.override_redirect = winPtr->atts.override_redirect;
    if (winPtr->changes.stack_mode == Above) {
	event.xconfigure.above = winPtr->changes.sibling;
    } else {
	event.xconfigure.above = None;
    }

    if (!(flags & TK_LOCATION_CHANGED)) {
	x = Tk_X(tkwin);
	y = Tk_Y(tkwin);
    }
    if (!(flags & TK_SIZE_CHANGED)) {
	width = Tk_Width(tkwin);
	height = Tk_Height(tkwin);
    }
    event.xconfigure.x = x;
    event.xconfigure.y = y;
    event.xconfigure.width = width;
    event.xconfigure.height = height;

    if (flags & TK_MACOSX_HANDLE_EVENT_IMMEDIATELY) {
	Tk_HandleEvent(&event);
    } else {
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    }

    /*
     * Update window manager information.
     */

    if (Tk_IsTopLevel(winPtr)) {
	wmPtr = winPtr->wmInfoPtr;
	if (flags & TK_LOCATION_CHANGED) {
	    wmPtr->x = x;
	    wmPtr->y = y;
	}
	if ((flags & TK_SIZE_CHANGED) && !(wmPtr->flags & WM_SYNC_PENDING) &&
		((width != Tk_Width(tkwin)) || (height != Tk_Height(tkwin)))) {
	    if ((wmPtr->width == -1) && (width == winPtr->reqWidth)) {
		/*
		 * Don't set external width, since the user didn't change it
		 * from what the widgets asked for.
		 */
	    } else if (wmPtr->gridWin != NULL) {
		wmPtr->width = wmPtr->reqGridWidth
			+ (width - winPtr->reqWidth)/wmPtr->widthInc;
		if (wmPtr->width < 0) {
		    wmPtr->width = 0;
		}
	    } else {
		wmPtr->width = width;
	    }

	    if ((wmPtr->height == -1) && (height == winPtr->reqHeight)) {
		/*
		 * Don't set external height, since the user didn't change it
		 * from what the widgets asked for.
		 */
	    } else if (wmPtr->gridWin != NULL) {
		wmPtr->height = wmPtr->reqGridHeight
			+ (height - winPtr->reqHeight)/wmPtr->heightInc;
		if (wmPtr->height < 0) {
		    wmPtr->height = 0;
		}
	    } else {
		wmPtr->height = height;
	    }

	    wmPtr->configWidth = width;
	    wmPtr->configHeight = height;
	}
    }

    /*
     * Now set up the changes structure. Under X we wait for the
     * ConfigureNotify to set these values. On the Mac we know immediately that
     * this is what we want - so we just set them. However, we need to make
     * sure the windows clipping region is marked invalid so the change is
     * visible to the subwindow.
     */

    winPtr->changes.x = x;
    winPtr->changes.y = y;
    winPtr->changes.width = width;
    winPtr->changes.height = height;
    TkMacOSXInvalClipRgns(tkwin);
}

/*
 *----------------------------------------------------------------------
 *
 * TkGenWMDestroyEvent --
 *
 *	Generate a WM Destroy event for Tk.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A WM_PROTOCOL/WM_DELETE_WINDOW event is sent to Tk.
 *
 *----------------------------------------------------------------------
 */

void
TkGenWMDestroyEvent(
    Tk_Window tkwin)
{
    XEvent event;

    event.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
    event.xany.send_event = False;
    event.xany.display = Tk_Display(tkwin);

    event.xclient.window = Tk_WindowId(tkwin);
    event.xclient.type = ClientMessage;
    event.xclient.message_type = Tk_InternAtom(tkwin, "WM_PROTOCOLS");
    event.xclient.format = 32;
    event.xclient.data.l[0] = Tk_InternAtom(tkwin, "WM_DELETE_WINDOW");
    Tk_HandleEvent(&event);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmProtocolEventProc --
 *
 *	This procedure is called by the Tk_HandleEvent whenever a ClientMessage
 *	event arrives whose type is "WM_PROTOCOLS". This procedure handles the
 *	message from the window manager in an appropriate fashion.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what sort of handler, if any, was set up for the protocol.
 *
 *----------------------------------------------------------------------
 */

void
TkWmProtocolEventProc(
    TkWindow *winPtr,		/* Window to which the event was sent. */
    XEvent *eventPtr)		/* X event. */
{
    WmInfo *wmPtr;
    ProtocolHandler *protPtr;
    Tcl_Interp *interp;
    Atom protocol;
    int result;

    wmPtr = winPtr->wmInfoPtr;
    if (wmPtr == NULL) {
	return;
    }
    protocol = (Atom) eventPtr->xclient.data.l[0];
    for (protPtr = wmPtr->protPtr; protPtr != NULL;
	    protPtr = protPtr->nextPtr) {
	if (protocol == protPtr->protocol) {
	    Tcl_Preserve(protPtr);
	    interp = protPtr->interp;
	    Tcl_Preserve(interp);
	    result = Tcl_EvalEx(interp, protPtr->command, -1, TCL_EVAL_GLOBAL);
	    if (result != TCL_OK) {
		Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
			"\n    (command for \"%s\" window manager protocol)",
			Tk_GetAtomName((Tk_Window)winPtr, protocol)));
		Tcl_BackgroundException(interp, result);
	    }
	    Tcl_Release(interp);
	    Tcl_Release(protPtr);
	    return;
	}
    }

    /*
     * No handler was present for this protocol. If this is a WM_DELETE_WINDOW
     * message then just destroy the window.
     */

    if (protocol == Tk_InternAtom((Tk_Window)winPtr, "WM_DELETE_WINDOW")) {
	Tk_DestroyWindow((Tk_Window)winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MacOSXIsAppInFront --
 *
 *	Returns 1 if this app is the foreground app.
 *
 * Results:
 *	1 if app is in front, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tk_MacOSXIsAppInFront(void)
{
    return ([NSRunningApplication currentApplication].active == true);
}

#pragma mark TKContentView

#import <ApplicationServices/ApplicationServices.h>

/*
 * Custom content view for use in Tk NSWindows.
 *
 * Since Tk handles all drawing of widgets, we only use the AppKit event loop
 * as a source of input events.  To do this, we overload the NSView drawRect
 * method with a method which generates Expose events for Tk but does no
 * drawing.  The redrawing operations are then done when Tk processes these
 * events.
 *
 * Earlier versions of Mac Tk used subclasses of NSView, e.g. NSButton, as the
 * basis for Tk widgets.  These would then appear as subviews of the
 * TKContentView.  To prevent the AppKit from redrawing and corrupting the Tk
 * Widgets it was necessary to use Apple private API calls.  In order to avoid
 * using private API calls, the NSView-based widgets have been replaced with
 * normal Tk widgets which draw themselves as native widgets by using the
 * HITheme API.
 *
 */

/*
 * Restrict event processing to Expose events.
 */

static Tk_RestrictAction
ExposeRestrictProc(
    ClientData arg,
    XEvent *eventPtr)
{
    return (eventPtr->type==Expose && eventPtr->xany.serial==PTR2UINT(arg)
	    ? TK_PROCESS_EVENT : TK_DEFER_EVENT);
}

/*
 * Restrict event processing to ConfigureNotify events.
 */

static Tk_RestrictAction
ConfigureRestrictProc(
    TCL_UNUSED(void *),
    XEvent *eventPtr)
{
    return (eventPtr->type==ConfigureNotify ? TK_PROCESS_EVENT : TK_DEFER_EVENT);
}

@implementation TKContentView(TKWindowEvent)

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
	/*
	 * The layer must exist before we set wantsLayer to YES.
	 */

	self.layer = [CALayer layer];
	self.wantsLayer = YES;
	self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
	self.layer.contentsGravity = self.layer.contentsAreFlipped ?
	    kCAGravityTopLeft : kCAGravityBottomLeft;

	/*
	 * Nothing gets drawn at all if the layer does not have a delegate.
	 * Currently, we do not implement any methods of the delegate, however.
	 */

	self.layer.delegate = (id) self;
    }
    return self;
}

/*
 * We will just use drawRect.
 */

- (BOOL) wantsUpdateLayer
{
    return NO;
}

- (void) viewDidChangeBackingProperties
{

    /*
     * Make sure that the layer uses a contentScale that matches the
     * backing scale factor of the screen.  This avoids blurry text whe
     * the view is on a Retina display, as well as incorrect size when
     * the view is on a normal display.
     */

    self.layer.contentsScale = self.window.screen.backingScaleFactor;
}

- (void) addTkDirtyRect: (NSRect) rect
{
    _tkNeedsDisplay = YES;
    _tkDirtyRect = NSUnionRect(_tkDirtyRect, rect);
    [NSApp setNeedsToDraw:YES];
    [self setNeedsDisplay:YES];
    [[self layer] setNeedsDisplay];
}

- (void) clearTkDirtyRect
{
    _tkNeedsDisplay = NO;
    _tkDirtyRect = NSZeroRect;
    [NSApp setNeedsToDraw:NO];
}

- (void) drawRect: (NSRect) rect
{
    (void)rect;

#ifdef TK_MAC_DEBUG_DRAWING
    TkWindow *winPtr = TkMacOSXGetTkWindow([self window]);
    if (winPtr) {
	fprintf(stderr, "drawRect: drawing %s in %s\n",
	    Tk_PathName(winPtr), NSStringFromRect(rect).UTF8String);
    }
#endif

    /*
     * We do not allow recursive calls to drawRect, but we only log them on OSX
     * > 10.13, where they should never happen.
     */

    if ([NSApp isDrawing]) {
	if ([NSApp macOSVersion] > 101300) {
	    TKLog(@"WARNING: a recursive call to drawRect was aborted.");
	}
	return;
    }

    [NSApp setIsDrawing: YES];
    [self clearTkDirtyRect];
    [self generateExposeEvents:rect];
    [NSApp setIsDrawing:NO];

#ifdef TK_MAC_DEBUG_DRAWING
    fprintf(stderr, "drawRect: done.\n");
#endif
}

-(void) setFrameSize: (NSSize)newsize
{
    [super setFrameSize: newsize];
    NSWindow *w = [self window];
    TkWindow *winPtr = TkMacOSXGetTkWindow(w);
    Tk_Window tkwin = (Tk_Window)winPtr;

    if (![self inLiveResize] &&
	[w respondsToSelector: @selector (tkLayoutChanged)]) {
	[(TKWindow *)w tkLayoutChanged];
    }

    if (winPtr) {
	unsigned int width = (unsigned int)newsize.width;
	unsigned int height=(unsigned int)newsize.height;
	ClientData oldArg;
    	Tk_RestrictProc *oldProc;

	/*
	 * This can be called from outside the Tk event loop.  Since it calls
	 * Tcl_DoOneEvent, we need to make sure we don't clobber the
	 * AutoreleasePool set up by the caller.
	 */

	[NSApp _lockAutoreleasePool];

	/*
	 * Disable Tk drawing until the window has been completely configured.
	 */

	TkMacOSXSetDrawingEnabled(winPtr, 0);

	 /*
	  * Generate and handle a ConfigureNotify event for the new size.
	  */

	TkGenWMConfigureEvent(tkwin, Tk_X(tkwin), Tk_Y(tkwin), width, height,
		TK_SIZE_CHANGED | TK_MACOSX_HANDLE_EVENT_IMMEDIATELY);
    	oldProc = Tk_RestrictEvents(ConfigureRestrictProc, NULL, &oldArg);
    	Tk_RestrictEvents(oldProc, oldArg, &oldArg);

	/*
	 * Now that Tk has configured all subwindows, create the clip regions.
	 */

	TkMacOSXSetDrawingEnabled(winPtr, 1);
	TkMacOSXInvalClipRgns(tkwin);
	TkMacOSXUpdateClipRgn(winPtr);

	 /*
	  * Generate and process expose events to redraw the window.  To avoid
	  * crashes, only do this if we are being called from drawRect.  See
	  * ticket [1fa8c3ed8d].
	  */

	if([NSApp isDrawing] || [self inLiveResize]) {
	    [self generateExposeEvents: [self bounds]];
	}

	/*
	 * Finally, unlock the main autoreleasePool.
	 */

	[NSApp _unlockAutoreleasePool];
    }
}

/*
 * Core method of this class: generates expose events for redrawing.  The
 * expose events are immediately removed from the Tcl event loop and processed.
 * This causes drawing procedures to be scheduled as idle events.  Then all
 * pending idle events are processed so the drawing will actually take place.
 */

- (void) generateExposeEvents: (NSRect) rect
{
    unsigned long serial;
    int updatesNeeded;
    CGRect updateBounds;
    TkWindow *winPtr = TkMacOSXGetTkWindow([self window]);
    ClientData oldArg;
    Tk_RestrictProc *oldProc;
    if (!winPtr) {
	return;
    }

    /*
     * Generate Tk Expose events.  All of these events will share the same
     * serial number.
     */

    updateBounds = NSRectToCGRect(rect);
    updateBounds.origin.y = ([self bounds].size.height - updateBounds.origin.y
			     - updateBounds.size.height);
    updatesNeeded = GenerateUpdates(&updateBounds, winPtr);
    if (updatesNeeded) {

	serial = LastKnownRequestProcessed(Tk_Display(winPtr));

	/*
	 * Use the ExposeRestrictProc to process only the expose events.  This
	 * will create idle drawing tasks, which we handle before we return.
	 */

    	oldProc = Tk_RestrictEvents(ExposeRestrictProc, UINT2PTR(serial), &oldArg);
    	while (Tcl_ServiceEvent(TCL_WINDOW_EVENTS|TCL_DONT_WAIT)) {};
    	Tk_RestrictEvents(oldProc, oldArg, &oldArg);

	/*
	 * Starting with OSX 10.14, which uses Core Animation to draw windows,
	 * all drawing must be done within the drawRect method.  (The CGContext
	 * which draws to the backing CALayer is created by the NSView before
	 * calling drawRect, and destroyed when drawRect returns.  Drawing done
	 * with the current CGContext outside of the drawRect method has no
	 * effect.)
	 *
	 * Fortunately, Tk schedules all drawing to be done while Tcl is idle.
	 * So to run any display procs which were scheduled by the expose
	 * events we process all idle events before returning.
	 */

	while (Tcl_DoOneEvent(TCL_IDLE_EVENTS)) {}
    }
}

/*
 * In macOS 10.14 and later this method is called when a user changes between
 * light and dark mode or changes the accent color. The implementation
 * generates two virtual events.  The first is either <<LightAqua>> or
 * <<DarkAqua>>, depending on the view's current effective appearance.  The
 * second is <<AppearnceChanged>> and has a data string describing the
 * effective appearance of the view and the current accent and highlight
 * colors.
 */

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101400

static const char *const accentNames[] = {
    "Graphite",
    "Red",
    "Orange",
    "Yellow",
    "Green",
    "Blue",
    "Purple",
    "Pink"
};

- (void) viewDidChangeEffectiveAppearance
{
    Tk_Window tkwin = (Tk_Window)TkMacOSXGetTkWindow([self window]);
    if (!tkwin) {
	return;
    }
    NSAppearanceName effectiveAppearanceName = [[self effectiveAppearance] name];
    NSUserDefaults *preferences = [NSUserDefaults standardUserDefaults];
    static const char *defaultColor = NULL;

    if (effectiveAppearanceName == NSAppearanceNameAqua) {
	TkSendVirtualEvent(tkwin, "LightAqua", NULL);
    } else if (effectiveAppearanceName == NSAppearanceNameDarkAqua) {
	TkSendVirtualEvent(tkwin, "DarkAqua", NULL);
    }
    if ([NSApp macOSVersion] < 101500) {

	/*
	 * Mojave cannot handle the KVO shenanigans that we need for the
	 * highlight and accent color notifications.
	 */

	return;
    }
    if (!defaultColor) {
	defaultColor = [NSApp macOSVersion] < 110000 ? "Blue" : "Multicolor";
	preferences = [[NSUserDefaults standardUserDefaults] retain];

	/*
	 * AppKit calls this method when the user changes the Accent Color
	 * but not when the user changes the Highlight Color.  So we register
	 * to receive KVO notifications for Highlight Color as well.
	 */

	[preferences addObserver:self
		      forKeyPath:@"AppleHighlightColor"
			 options:NSKeyValueObservingOptionNew
			 context:NULL];
    }
    NSString *accent = [preferences stringForKey:@"AppleAccentColor"];
    NSArray *words = [[preferences stringForKey:@"AppleHighlightColor"]
			        componentsSeparatedByString: @" "];
    NSString *highlight = [words count] > 3 ? [words objectAtIndex:3] : nil;
    const char *accentName = accent ? accentNames[1 + accent.intValue] : defaultColor;
    const char *highlightName = highlight ? highlight.UTF8String: defaultColor;
    char data[256];
    snprintf(data, 256, "Appearance %s Accent %s Highlight %s",
	     effectiveAppearanceName.UTF8String, accentName,
	     highlightName);
    TkSendVirtualEvent(tkwin, "AppearanceChanged", Tcl_NewStringObj(data, -1));
}

- (void)observeValueForKeyPath:(NSString *)keyPath
		      ofObject:(id)object
			change:(NSDictionary *)change
		       context:(void *)context
{
    NSUserDefaults *preferences = [NSUserDefaults standardUserDefaults];
    if (object == preferences && [keyPath isEqualToString:@"AppleHighlightColor"]) {
	if (@available(macOS 10.14, *)) {
	    [self viewDidChangeEffectiveAppearance];
	}
    }
}

#endif

/*
 * This is no-op on 10.7 and up because Apple has removed this widget, but we
 * are leaving it here for backwards compatibility.
 */

- (void) tkToolbarButton: (id) sender
{
#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd);
#endif
    XVirtualEvent event;
    int x, y;
    TkWindow *winPtr = TkMacOSXGetTkWindow([self window]);
    Tk_Window tkwin = (Tk_Window)winPtr;
    (void)sender;

    if (!winPtr){
	return;
    }
    bzero(&event, sizeof(XVirtualEvent));
    event.type = VirtualEvent;
    event.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
    event.send_event = false;
    event.display = Tk_Display(tkwin);
    event.event = Tk_WindowId(tkwin);
    event.root = XRootWindow(Tk_Display(tkwin), 0);
    event.subwindow = None;
    event.time = TkpGetMS();
    XQueryPointer(NULL, winPtr->window, NULL, NULL,
	    &event.x_root, &event.y_root, &x, &y, &event.state);
    Tk_TopCoordsToWindow(tkwin, x, y, &event.x, &event.y);
    event.same_screen = true;
    event.name = Tk_GetUid("ToolbarButton");
    Tk_QueueWindowEvent((XEvent *) &event, TCL_QUEUE_TAIL);
}

/*
 * On Catalina this is never called and drawRect clips to the rect that
 * is passed to it by AppKit.
 */

- (BOOL) wantsDefaultClipping
{
    return NO;
}

- (BOOL) acceptsFirstResponder
{
    return YES;
}

/*
 * This keyDown method does nothing, which is a huge improvement over the
 * default keyDown method which beeps every time a key is pressed.
 */

- (void) keyDown: (NSEvent *) theEvent
{
    (void)theEvent;

#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
#endif
}

/*
 * When the services menu is opened this is called for each Responder in
 * the Responder chain until a service provider is found.  The TKContentView
 * should be the first (and generally only) Responder in the chain.  We
 * return the TkServices object that was created in TkpInit.
 */

- (id)validRequestorForSendType:(NSString *)sendType
		     returnType:(NSString *)returnType
{
    if ([sendType isEqualToString:@"NSStringPboardType"] ||
	[sendType isEqualToString:@"NSPasteboardTypeString"]) {
	return [NSApp servicesProvider];
    }
    return [super validRequestorForSendType:sendType returnType:returnType];
}

@end

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
