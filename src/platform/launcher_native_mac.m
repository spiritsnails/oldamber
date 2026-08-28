/* launcher_native_mac.m: launcher_native.h on macOS, via NSOpenPanel.
 *
 * Windows has had the browser's "..." button since it was written
 * (GetOpenFileNameW, launcher_native.c). Every other platform got the stub, so
 * on a Mac the button simply never appeared, LauncherBrowse_Run draws it only
 * when LauncherNative_HasFileDialog() says there is something behind it.
 *
 * The stub's comment justifies itself with the handheld case: a system dialog
 * cannot be driven with a controller. True, and already handled one level up:
 * launcher_browse.c gates the button on `pointer && HasFileDialog()`, so a Deck
 * never shows it whatever this returns. A Mac is a desktop with a mouse, and it
 * was only missing the control because nobody had written this file.
 *
 * Objective-C because NSOpenPanel is Cocoa. Kept in its own translation unit so
 * launcher_native.c stays plain C, and the same rule its header states,
 * nothing from hardware.h directly or transitively, is just as easy to hold.
 */
#include "launcher_native.h"

#import <Cocoa/Cocoa.h>
#include <string.h>

int LauncherNative_HasFileDialog(void) { return 1; }

/* "Game Boy ROMs|*.gb;*.gbc|All files|*.*|" -> {"gb", "gbc"}.
 *
 * The filter is GetOpenFileNameW-shaped because that is what the header
 * specifies and what the one caller passes; NSOpenPanel wants bare extensions,
 * so pull them out of the odd (pattern) fields and drop the labels. "*.*" is
 * skipped: in Cocoa "allow anything" is expressed by setting no list at all,
 * and a literal "*" extension would match nothing. */
static NSArray *extensions_from_filter(const char *filter) {
    NSMutableArray *out = [NSMutableArray array];
    int field = 0;              /* even = label, odd = patterns */
    const char *p = filter;

    if (!filter) return out;
    while (*p) {
        const char *end = strchr(p, '|');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (field & 1) {
            /* Split this field on ';' and take what follows each "*." */
            size_t i = 0;
            while (i < len) {
                size_t j = i;
                while (j < len && p[j] != ';') j++;
                {
                    const char *tok = p + i;
                    size_t tlen = j - i;
                    if (tlen > 2 && tok[0] == '*' && tok[1] == '.' &&
                        !(tlen == 3 && tok[2] == '*')) {
                        NSString *ext =
                            [[NSString alloc] initWithBytes:tok + 2
                                                     length:tlen - 2
                                                   encoding:NSUTF8StringEncoding];
                        if (ext) [out addObject:ext];
                    }
                }
                i = (j < len) ? j + 1 : len;
            }
        }
        field++;
        if (!end) break;
        p = end + 1;
    }
    return out;
}

int LauncherNative_BrowseFile(SDL_Window *win, const char *title,
                              const char *filter, char *out_path, size_t out_sz) {
    __block int ok = 0;

    /* The window is unused here. AppKit owns modality for a panel run this way,
     * and SDL's window is the app's only window anyway, unlike the Windows
     * path, which needs an HWND to parent the dialog to. */
    (void)win;

    if (!out_path || out_sz == 0) return 0;
    out_path[0] = '\0';

    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        NSArray *exts = extensions_from_filter(filter);

        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setResolvesAliases:YES];
        /* A player keeping ROMs in a hidden folder is not this dialog's to
         * refuse, and the built-in browser shows them too. */
        [panel setShowsHiddenFiles:NO];
        if (title)
            [panel setMessage:[NSString stringWithUTF8String:title]];
        if ([exts count] > 0)
            [panel setAllowedFileTypes:exts];

        /* The app has to be frontmost or the panel opens behind the game.
         * SDL does not necessarily leave the process activated, and a modal
         * nobody can see is the same bug as no dialog at all. */
        [NSApp activateIgnoringOtherApps:YES];

        if ([panel runModal] == NSModalResponseOK) {
            NSURL *url = [[panel URLs] firstObject];
            const char *utf8 = url ? [[url path] UTF8String] : NULL;
            if (utf8 && utf8[0]) {
                strncpy(out_path, utf8, out_sz - 1);
                out_path[out_sz - 1] = '\0';
                ok = 1;
            }
        }
    }
    return ok;
}
