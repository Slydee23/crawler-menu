package crawlermenu;

import android.util.Log;

/** Calls into libcrawlermenu.so (already loaded by the patched Application class). */
final class NativeBridge {
    // Must match features::Id in native/src/features.h
    static final int COIN_MULTIPLIER = 1;

    private NativeBridge() {}

    private static native void setFeature(int id, boolean enabled);

    /** Never throws: a missing native side must not crash the game. */
    static void set(int id, boolean enabled) {
        try {
            setFeature(id, enabled);
        } catch (Throwable t) {
            Log.e(Loader.TAG, "setFeature(" + id + ") failed", t);
        }
    }
}
