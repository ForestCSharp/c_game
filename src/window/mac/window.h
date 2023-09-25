#pragma once

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

// FCS TODO: make per-window
bool global_key_states[KEY_MAX_VALUE];

KeyCode translate_macos_key_code(unsigned short key_code)
{
    switch (key_code)
    {
    case kVK_ANSI_A:
        return 'A';
    case kVK_ANSI_B:
        return 'B';
    case kVK_ANSI_C:
        return 'C';
    case kVK_ANSI_D:
        return 'D';
    case kVK_ANSI_E:
        return 'E';
    case kVK_ANSI_F:
        return 'F';
    case kVK_ANSI_G:
        return 'G';
    case kVK_ANSI_H:
        return 'H';
    case kVK_ANSI_I:
        return 'I';
    case kVK_ANSI_J:
        return 'J';
    case kVK_ANSI_K:
        return 'K';
    case kVK_ANSI_L:
        return 'L';
    case kVK_ANSI_M:
        return 'M';
    case kVK_ANSI_N:
        return 'N';
    case kVK_ANSI_O:
        return 'O';
    case kVK_ANSI_P:
        return 'P';
    case kVK_ANSI_Q:
        return 'Q';
    case kVK_ANSI_R:
        return 'R';
    case kVK_ANSI_S:
        return 'S';
    case kVK_ANSI_T:
        return 'T';
    case kVK_ANSI_U:
        return 'U';
    case kVK_ANSI_V:
        return 'V';
    case kVK_ANSI_W:
        return 'W';
    case kVK_ANSI_X:
        return 'X';
    case kVK_ANSI_Y:
        return 'Y';
    case kVK_ANSI_Z:
        return 'Z';
    // FCS TODO: the rest of the kVK_ANSI_... values
    case kVK_Escape:
        return KEY_ESCAPE;
    case kVK_Space:
        return KEY_SPACE;
        // FCS TODO: the rest of the kVK_... values
    }

    return KEY_MAX_VALUE;
}

@interface WindowView : NSView
{
    NSTrackingArea* trackingArea;
  @public
    i32 cached_mouse_x;
  @public
    i32 cached_mouse_y;
}
@end

@implementation WindowView

- (instancetype)init
{
    if ((self = [super init])) // 'super' used to access methods from parent class
    {
        const NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                                              NSEventMaskLeftMouseDragged | NSEventMaskRightMouseDragged |
                                              NSEventMaskOtherMouseDragged | NSTrackingActiveInKeyWindow |
                                              NSTrackingEnabledDuringMouseDrag | NSTrackingCursorUpdate |
                                              NSTrackingInVisibleRect | NSTrackingAssumeInside;

        trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds] options:options owner:self userInfo:nil];

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

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)keyDown:(NSEvent*)event
{
    KeyCode key_code = translate_macos_key_code([event keyCode]);
    if (key_code != KEY_MAX_VALUE)
    {
        global_key_states[key_code] = true;
    }
}

- (void)keyUp:(NSEvent*)event
{
    KeyCode key_code = translate_macos_key_code([event keyCode]);
    if (key_code != KEY_MAX_VALUE)
    {
        global_key_states[key_code] = false;
    }
}

- (void)flagsChanged:(NSEvent*)event
{
    if ([event modifierFlags] & NSEventModifierFlagShift)
    {
        global_key_states[KEY_SHIFT] = true;
    }
    else
    {
        global_key_states[KEY_SHIFT] = false;
    }
}

- (void)mouseDown:(NSEvent*)event
{
    global_key_states[KEY_LEFT_MOUSE] = true;
}

- (void)mouseUp:(NSEvent*)event
{
    global_key_states[KEY_LEFT_MOUSE] = false;
}

- (void)rightMouseDown:(NSEvent*)event
{
    global_key_states[KEY_RIGHT_MOUSE] = true;
}

- (void)rightMouseUp:(NSEvent*)event
{
    global_key_states[KEY_RIGHT_MOUSE] = false;
}

- (void)mouseMoved:(NSEvent*)event
{
    NSPoint mouse_pos = event.locationInWindow;
    NSSize view_size = self.frame.size;
    cached_mouse_x = (i32)mouse_pos.x;
    // Note: In Cocoa, (0,0) is bottom left instead of top left like in Win32
    cached_mouse_y = view_size.height - (i32)mouse_pos.y - 1;
}

- (void)mouseDragged:(NSEvent*)event
{
    [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent*)event
{
    [self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent*)event
{
    [self mouseMoved:event];
}
@end

typedef struct Window
{
    NSWindow* ns_window;
    WindowView* ns_view;
    CAMetalLayer* metal_layer;
} Window;

Window window_create(const char* name, int width, int height)
{
    @autoreleasepool
    {
        // FCS TODO: Only create app once
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect frame = NSMakeRect(0, 0, width, height);
        const NSUInteger window_style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        NSWindow* window = [[[NSWindow alloc] initWithContentRect:frame
                                                        styleMask:window_style
                                                          backing:NSBackingStoreBuffered
                                                            defer:NO] autorelease];
        [window setBackgroundColor:[NSColor blueColor]];
        [window makeKeyAndOrderFront:window];

        NSString* ns_string_name = [[NSString alloc] initWithUTF8String:name];
        [window setTitle:ns_string_name];

        WindowView* view = [[[WindowView alloc] init] autorelease];
        [window setContentView:view];

        CAMetalLayer* layer = [[CAMetalLayer alloc] init];
        [view setWantsLayer:YES];
        [view setLayer:layer];

        [NSApp activateIgnoringOtherApps:YES];

        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantFuture]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        [NSApp sendEvent:event];

        return (Window){
            .ns_window = window,
            .ns_view = view,
            .metal_layer = layer,
        };
    }
}

bool window_handle_messages(const Window* const window)
{
    @autoreleasepool
    {
        NSEvent* ev;
        do
        {
            ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
            if (ev)
            {
                // handle events here
                [NSApp sendEvent:ev];
            }
        } while (ev);
    }
    return true; // FCS TODO: return false on quit event
}

void window_get_dimensions(const Window* const window, int* out_width, int* out_height)
{
    @autoreleasepool
    {
        NSSize ns_size = [[window->ns_window contentView] frame].size;
        *out_width = ns_size.width;
        *out_height = ns_size.height;
    }
}

void window_get_mouse_pos(const Window* const window, i32* out_mouse_x, i32* out_mouse_y)
{
    *out_mouse_x = window->ns_view->cached_mouse_x;
    *out_mouse_y = window->ns_view->cached_mouse_y;
}

bool input_pressed(int key_code)
{
    return global_key_states[key_code];
}
