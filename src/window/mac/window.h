#pragma once

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

//FCS TODO: Hacky test, make per-window
bool global_key_states[1024];
bool mouse_button_states[3];

@interface WindowView : NSView
{
	NSTrackingArea* trackingArea;
	@public int32_t cached_mouse_x;
	@public int32_t cached_mouse_y;
}
@end

@implementation WindowView

- (instancetype)init
{
	if ((self = [super init])) // 'super' used to access methods from parent class
	{
		const NSTrackingAreaOptions options = 
			NSTrackingMouseEnteredAndExited |
			NSTrackingMouseMoved |
			NSEventMaskLeftMouseDragged |
            NSEventMaskRightMouseDragged |
			NSEventMaskOtherMouseDragged |
			NSTrackingActiveInKeyWindow |
			NSTrackingEnabledDuringMouseDrag |
			NSTrackingCursorUpdate |
			NSTrackingInVisibleRect |
			NSTrackingAssumeInside;

		trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
			options:options
			owner:self
			userInfo:nil];

		[self addTrackingArea:trackingArea];
		[super updateTrackingAreas];

	}
	return self;
}

- (void)dealloc
{
	[trackingArea release];
	[super dealloc];
}

- (BOOL)canBecomeKeyView {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent*)event {
	unsigned short key_code = [event keyCode];
	global_key_states[key_code] = true;
	printf("key code: %u\n", key_code);
}

- (void)keyUp:(NSEvent*)event {
	unsigned short key_code = [event keyCode];
	global_key_states[key_code] = false;
}

- (void)mouseDown:(NSEvent*)event {
	mouse_button_states[0] = true;	
}

- (void)mouseUp:(NSEvent*)event {
	mouse_button_states[0] = false;	
}

- (void)rightMouseDown:(NSEvent*)event {
	mouse_button_states[1] = true;	
}

- (void)rightMouseUp:(NSEvent*)event {
	mouse_button_states[1] = false;	
}

- (void)mouseMoved:(NSEvent*)event {
	NSPoint mouse_pos = event.locationInWindow;
	NSSize view_size = self.frame.size;
	cached_mouse_x = (int32_t) mouse_pos.x;
	// Note: In Cocoa, (0,0) is bottom left instead of top left like in Win32
	cached_mouse_y = view_size.height - (int32_t) mouse_pos.y - 1;
}

- (void)mouseDragged:(NSEvent*)event {
	[self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent*)event {
	[self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent*)event {
	[self mouseMoved:event];
}

@end

typedef struct Window {
	NSWindow* ns_window;
	WindowView* ns_view;
	CAMetalLayer* metal_layer;
} Window;

Window window_create(const char* name, int width, int height)
{	
	[NSAutoreleasePool new];

	//FCS TODO: Only create app once
	NSApplication* app = [NSApplication sharedApplication];
	[app setActivationPolicy:NSApplicationActivationPolicyRegular];

	NSRect frame = NSMakeRect(0, 0, width, height);
	const NSUInteger window_style = NSWindowStyleMaskTitled |
		        					NSWindowStyleMaskClosable |
									NSWindowStyleMaskMiniaturizable |
									NSWindowStyleMaskResizable;
	NSWindow* window  = [[[NSWindow alloc] initWithContentRect:frame
		styleMask:window_style
		backing:NSBackingStoreBuffered
		defer:NO] autorelease];
	[window setBackgroundColor:[NSColor blueColor]];
	[window makeKeyAndOrderFront:window];
	
	NSString *ns_string_name = [[NSString alloc] initWithUTF8String: name];
	[window setTitle:ns_string_name ];

	WindowView* view = [[[WindowView alloc] init] autorelease];
	[window setContentView:view];

	CAMetalLayer* layer = [[CAMetalLayer alloc] init];
	[view setWantsLayer:YES];
	[view setLayer:layer];

	[NSApp activateIgnoringOtherApps:YES];
	
	NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
		untilDate:[NSDate distantFuture]
		inMode:NSDefaultRunLoopMode
		dequeue:YES];
	[NSApp sendEvent:event];
		
	return (Window) {
		.ns_window = window,
		.ns_view = view,
		.metal_layer = layer,
	};
}

bool window_handle_messages(const Window* const window)
{
	@autoreleasepool {
        NSEvent* ev;
        do {
            ev = [NSApp nextEventMatchingMask: NSEventMaskAny
                                    untilDate: nil
                                       inMode: NSDefaultRunLoopMode
                                      dequeue: YES];
            if (ev) {
                // handle events here
                [NSApp sendEvent: ev];
            }
        } while (ev);
    }
	return true; //FCS TODO: return false on quit event
}

void window_get_dimensions(const Window* const window, int* out_width, int* out_height)
{
	NSSize ns_size = [[window->ns_window contentView] frame].size;
	*out_width = ns_size.width;
	*out_height = ns_size.height;
}

void window_get_mouse_pos(const Window* const window, int32_t* out_mouse_x, int32_t* out_mouse_y)
{
	*out_mouse_x = window->ns_view->cached_mouse_x;
	*out_mouse_y = window->ns_view->cached_mouse_y;
}

//FCS TODO: per-platform keycode translation function, rather than all these constants.
static const int KEY_ESCAPE = 53; //TODO:
static const int KEY_SHIFT = 57; //TODO:
static const int KEY_SPACE = 49; //TODO:

static const int KEY_LEFT_MOUSE = 201; //TODO:
static const int KEY_RIGHT_MOUSE = 202; //TODO:
static const int KEY_MIDDLE_MOUSE = 203; //TODO:

//FCS TODO: Make input per-window
bool input_pressed(int key_code) {
	if (key_code == KEY_LEFT_MOUSE)
	{
		return mouse_button_states[0];
	}
	else if (key_code == KEY_RIGHT_MOUSE)
	{
		return mouse_button_states[1];
	}
	return global_key_states[key_code];
}

