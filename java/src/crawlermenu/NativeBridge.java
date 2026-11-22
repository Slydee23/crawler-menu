package crawlermenu;

import android.util.Log;

/** Calls into libcrawlermenu.so (already loaded by the patched Application class). */
final class NativeBridge {
    // Must match features::Id in native/src/feature_flags.h
    static final int COIN_MULTIPLIER = 1;
    static final int INFINITE_CHOICES = 2;
    static final int GODMODE = 3;
    static final int NO_MANA_COST = 4;
    static final int XP_MULTIPLIER = 5;
    static final int ONE_HIT_KILL = 6;
    static final int COMBO_NEVER_BREAKS = 7;

    private NativeBridge() {}

    private static native void setFeature(int id, boolean enabled);

    private static native void setFeatureValue(int id, int value);

    private static native String[] getStatLabels();

    private static native float[] getStatValues();

    private static native boolean setStat(int index, float value);

    /** Labels of the editable run stats, in menu order (empty if the native side is missing). */
    static String[] statLabels() {
        try {
            return getStatLabels();
        } catch (Throwable t) {
            Log.e(Loader.TAG, "getStatLabels failed", t);
            return new String[0];
        }
    }

    /** Current run stat values (NaN = unavailable), or null when no run is active. */
    static float[] statValues() {
        try {
            return getStatValues();
        } catch (Throwable t) {
            Log.e(Loader.TAG, "getStatValues failed", t);
            return null;
        }
    }

    /** Queues setting a run stat for the next game frame. False if no run is active. */
    static boolean queueStat(int index, float value) {
        try {
            return setStat(index, value);
        } catch (Throwable t) {
            Log.e(Loader.TAG, "setStat(" + index + ") failed", t);
            return false;
        }
    }

    /** Sets a feature's numeric setting (e.g. the coin multiplier). Never throws. */
    static void setValue(int id, int value) {
        try {
            setFeatureValue(id, value);
        } catch (Throwable t) {
            Log.e(Loader.TAG, "setFeatureValue(" + id + ") failed", t);
        }
    }

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
