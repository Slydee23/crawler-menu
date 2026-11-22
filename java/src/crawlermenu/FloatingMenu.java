package crawlermenu;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
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
import android.widget.SeekBar;
import android.widget.TextView;

/**
 * A draggable round button on top of the game. Tapping it opens/closes the menu panel, which can be
 * dragged by its title. Views are added to the activity's decor view, so no overlay permission
 * is needed. Built in code because the patched APK has no resources of ours.
 */
final class FloatingMenu {
    private static final String BUTTON_TAG = "crawlermenu.button";

    // Blue of the game's main menu buttons.
    private static final int ACCENT = Color.rgb(0x26, 0x3F, 0xCD);

    private static final String PREFS = "crawlermenu";
    private static final String PREF_PANEL_OPACITY = "panel_opacity";
    private static final int DEFAULT_PANEL_OPACITY = 92;  // percent, was alpha 235
    private static final int MIN_PANEL_OPACITY = 20;

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
        b.setOnTouchListener(new Dragger(b, this::togglePanel));
        return b;
    }

    private LinearLayout createPanel() {
        LinearLayout p = new LinearLayout(activity);
        p.setOrientation(LinearLayout.VERTICAL);
        p.setPadding(dp(20), dp(16), dp(20), dp(16));

        GradientDrawable bg = new GradientDrawable();
        bg.setCornerRadius(dp(12));
        bg.setStroke(dp(2), ACCENT);
        p.setBackground(bg);
        int opacity = prefs().getInt(PREF_PANEL_OPACITY, DEFAULT_PANEL_OPACITY);
        setPanelOpacity(bg, opacity);

        // The title is the drag handle for the whole panel.
        TextView title = new TextView(activity);
        title.setText("CrawlerMenu");
        title.setTextColor(Color.WHITE);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        title.setOnTouchListener(new Dragger(p, null));
        p.addView(title);

        TextView info = new TextView(activity);
        info.setText("No features yet.");
        info.setTextColor(Color.LTGRAY);
        info.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        info.setPadding(0, dp(8), 0, dp(12));
        p.addView(info);

        // Background opacity slider (text stays fully readable). Stored across game restarts.
        TextView opacityLabel = new TextView(activity);
        opacityLabel.setText(opacityText(opacity));
        opacityLabel.setTextColor(Color.LTGRAY);
        opacityLabel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        p.addView(opacityLabel);

        SeekBar slider = new SeekBar(activity);
        slider.setMax(100 - MIN_PANEL_OPACITY);
        slider.setProgress(opacity - MIN_PANEL_OPACITY);
        slider.setProgressTintList(ColorStateList.valueOf(ACCENT));
        slider.setThumbTintList(ColorStateList.valueOf(ACCENT));
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int progress, boolean fromUser) {
                int value = progress + MIN_PANEL_OPACITY;
                setPanelOpacity(bg, value);
                opacityLabel.setText(opacityText(value));
            }

            @Override public void onStartTrackingTouch(SeekBar s) {}

            @Override public void onStopTrackingTouch(SeekBar s) {
                prefs().edit().putInt(PREF_PANEL_OPACITY, s.getProgress() + MIN_PANEL_OPACITY).apply();
            }
        });
        LinearLayout.LayoutParams sliderLp = new LinearLayout.LayoutParams(dp(220), ViewGroup.LayoutParams.WRAP_CONTENT);
        sliderLp.bottomMargin = dp(8);
        p.addView(slider, sliderLp);

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

    private SharedPreferences prefs() {
        return activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    /** Sets the panel background's fill alpha; {@code percent} is 0..100. */
    private static void setPanelOpacity(GradientDrawable bg, int percent) {
        bg.setColor(Color.argb(Math.round(percent * 2.55f), 20, 20, 24));
    }

    private static String opacityText(int percent) {
        return "Background opacity: " + percent + "%";
    }

    private void togglePanel() {
        panel.setVisibility(panel.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
        button.bringToFront();
    }

    /**
     * Touch handler that drags {@code target} around the screen. A short tap without movement
     * runs {@code onTap} (if any). Works for any view whose layout params are FrameLayout params.
     */
    private final class Dragger implements View.OnTouchListener {
        private final View target;
        private final Runnable onTap;
        private final int slop = ViewConfiguration.get(activity).getScaledTouchSlop();
        private float downX;
        private float downY;
        private int startLeft;
        private int startTop;
        private boolean dragging;

        Dragger(View target, Runnable onTap) {
            this.target = target;
            this.onTap = onTap;
        }

        @Override public boolean onTouch(View v, MotionEvent e) {
            FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) target.getLayoutParams();
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    // Switch from e.g. centered placement to absolute coordinates.
                    if (lp.gravity != (Gravity.TOP | Gravity.START)) {
                        lp.gravity = Gravity.TOP | Gravity.START;
                        lp.leftMargin = target.getLeft();
                        lp.topMargin = target.getTop();
                        target.setLayoutParams(lp);
                    }
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
                        lp.leftMargin = clamp(startLeft + Math.round(dx), decor.getWidth() - target.getWidth());
                        lp.topMargin = clamp(startTop + Math.round(dy), decor.getHeight() - target.getHeight());
                        target.setLayoutParams(lp);
                    }
                    return true;
                case MotionEvent.ACTION_UP:
                    if (!dragging && onTap != null) onTap.run();
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
