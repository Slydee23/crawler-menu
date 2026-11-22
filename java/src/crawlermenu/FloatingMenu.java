package crawlermenu;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * A draggable round button on top of the game. Tapping it opens/closes the menu panel.
 * Views are added to the activity's decor view, so no overlay permission is needed.
 * Built in code because the patched APK has no resources of ours.
 */
final class FloatingMenu {
    private static final String BUTTON_TAG = "crawlermenu.button";
    private static final int ACCENT = Color.rgb(196, 30, 58);

    private final Activity activity;
    private final ViewGroup decor;
    private final LinearLayout panel;
    private final TextView button;

    static void attach(Activity activity) {
        ViewGroup decor = (ViewGroup) activity.getWindow().getDecorView();
        if (decor.findViewWithTag(BUTTON_TAG) != null) return;  // already attached
        new FloatingMenu(activity, decor);
        Log.i(Loader.TAG, "Menu attached to " + activity.getClass().getName());
    }

    private FloatingMenu(Activity activity, ViewGroup decor) {
        this.activity = activity;
        this.decor = decor;
        this.panel = createPanel();
        this.button = createButton();
        panel.setVisibility(View.GONE);
        decor.addView(panel);
        decor.addView(button);
    }

    private int dp(float value) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value,
                activity.getResources().getDisplayMetrics()));
    }

    private TextView createButton() {
        TextView b = new TextView(activity);
        b.setTag(BUTTON_TAG);
        b.setText("CM");
        b.setTextColor(Color.WHITE);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setGravity(Gravity.CENTER);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);

        GradientDrawable bg = new GradientDrawable();
        bg.setShape(GradientDrawable.OVAL);
        bg.setColor(Color.argb(200, 20, 20, 24));
        bg.setStroke(dp(2), ACCENT);
        b.setBackground(bg);

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(dp(48), dp(48));
        lp.gravity = Gravity.TOP | Gravity.START;
        lp.leftMargin = dp(16);
        lp.topMargin = dp(96);
        b.setLayoutParams(lp);
        b.setOnTouchListener(new DragOrTap());
        return b;
    }

    private LinearLayout createPanel() {
        LinearLayout p = new LinearLayout(activity);
        p.setOrientation(LinearLayout.VERTICAL);
        p.setPadding(dp(20), dp(16), dp(20), dp(16));

        GradientDrawable bg = new GradientDrawable();
        bg.setCornerRadius(dp(12));
        bg.setColor(Color.argb(235, 20, 20, 24));
        bg.setStroke(dp(2), ACCENT);
        p.setBackground(bg);

        TextView title = new TextView(activity);
        title.setText("CrawlerMenu");
        title.setTextColor(Color.WHITE);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        p.addView(title);

        TextView info = new TextView(activity);
        info.setText("No features yet.");
        info.setTextColor(Color.LTGRAY);
        info.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        info.setPadding(0, dp(8), 0, dp(12));
        p.addView(info);

        TextView close = new TextView(activity);
        close.setText("Close");
        close.setTextColor(ACCENT);
        close.setTypeface(Typeface.DEFAULT_BOLD);
        close.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        close.setPadding(0, dp(4), 0, dp(4));
        close.setOnClickListener(v -> panel.setVisibility(View.GONE));
        p.addView(close);

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.CENTER;
        p.setLayoutParams(lp);
        p.setClickable(true);  // taps on the panel must not reach the game
        return p;
    }

    private void togglePanel() {
        panel.setVisibility(panel.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
        button.bringToFront();
    }

    /** Moves the button when dragged, toggles the panel on a short tap. */
    private final class DragOrTap implements View.OnTouchListener {
        private final int slop = ViewConfiguration.get(activity).getScaledTouchSlop();
        private float downX;
        private float downY;
        private int startLeft;
        private int startTop;
        private boolean dragging;

        @Override public boolean onTouch(View v, MotionEvent e) {
            FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) v.getLayoutParams();
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    downX = e.getRawX();
                    downY = e.getRawY();
                    startLeft = lp.leftMargin;
                    startTop = lp.topMargin;
                    dragging = false;
                    return true;
                case MotionEvent.ACTION_MOVE:
                    float dx = e.getRawX() - downX;
                    float dy = e.getRawY() - downY;
                    if (!dragging && Math.hypot(dx, dy) > slop) dragging = true;
                    if (dragging) {
                        lp.leftMargin = clamp(startLeft + Math.round(dx), decor.getWidth() - v.getWidth());
                        lp.topMargin = clamp(startTop + Math.round(dy), decor.getHeight() - v.getHeight());
                        v.setLayoutParams(lp);
                    }
                    return true;
                case MotionEvent.ACTION_UP:
                    if (!dragging) togglePanel();
                    return true;
                default:
                    return true;
            }
        }

        private int clamp(int value, int max) {
            return Math.max(0, Math.min(value, Math.max(0, max)));
        }
    }
}
