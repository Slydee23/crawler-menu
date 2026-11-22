package crawlermenu;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.util.Log;

/**
 * Entry point called from the patched Application.attachBaseContext().
 * Waits for activities and puts the floating menu on top of them.
 */
public final class Loader {
    static final String TAG = "CrawlerMenu";
    private static boolean initialized;

    private Loader() {}

    public static void init(Application app) {
        if (initialized) return;
        initialized = true;
        try {
            app.registerActivityLifecycleCallbacks(new Callbacks());
            Log.i(TAG, "Java loader registered");
        } catch (Throwable t) {
            Log.e(TAG, "Java loader init failed", t);
        }
    }

    private static final class Callbacks implements Application.ActivityLifecycleCallbacks {
        @Override public void onActivityResumed(Activity activity) {
            try {
                FloatingMenu.attach(activity);
            } catch (Throwable t) {
                Log.e(TAG, "Could not attach menu", t);
            }
        }

        @Override public void onActivityCreated(Activity activity, Bundle state) {}
        @Override public void onActivityStarted(Activity activity) {}
        @Override public void onActivityPaused(Activity activity) {}
        @Override public void onActivityStopped(Activity activity) {}
        @Override public void onActivitySaveInstanceState(Activity activity, Bundle state) {}
        @Override public void onActivityDestroyed(Activity activity) {}
    }
}
