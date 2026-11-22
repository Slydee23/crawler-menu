package crawlermenu;

import android.util.Log;

/** Calls into libcrawlermenu.so (already loaded by the patched Application class). */
final class NativeBridge {
    // Must match features::Id in native/src/feature_flags.h
    static final int COIN_MULTIPLIER = 1;
    static final int INFINITE_CHOICES = 2;

    private NativeBridge() {}

    private static native void setFeature(int id, boolean enabled);

    // Results of queueCoins(), must match Java_crawlermenu_NativeBridge_addCoins
    static final int ADDED_NONE = 0;
    static final int ADDED_RUN = 1;
    static final int ADDED_VILLAGE = 2;

    private static native int addCoins(int amount);

    /**
     * Queues coins to be added on the next game frame: run coins during a run, village gold
     * otherwise. Returns one of the ADDED_* constants.
     */
    static int queueCoins(int amount) {
        try {
            return addCoins(amount);
        } catch (Throwable t) {
            Log.e(Loader.TAG, "addCoins(" + amount + ") failed", t);
            return ADDED_NONE;
        }
    }

    /** Never throws: a missing native side must not crash the game. */
    static void set(int id, boolean enabled) {
        try {
            setFeature(id, enabled);
        } catch (Throwable t) {
            Log.e(Loader.TAG, "setFeature(" + id + ") failed", t);
        }
    }
}
