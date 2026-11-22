package crawlermenu;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Switch;
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
    private static final String PREF_COIN_MULTIPLIER = "coin_multiplier";
    private static final int DEFAULT_COIN_MULTIPLIER = 10;

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
        // Switches are MATCH_PARENT and do not count when the panel sizes itself, so give the
        // panel a minimum width that fits the longest switch label.
        p.setMinimumWidth(dp(360));

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

        // Cheats (always off when the game starts)
        coinMultiplierRow(p);
        p.addView(featureSwitch("Infinite rerolls, skips & banishes", NativeBridge.INFINITE_CHOICES));
        p.addView(featureSwitch("Godmode", NativeBridge.GODMODE));
        addCoinsRow(p);

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

    /** "Coins × [factor]  (switch)" — the factor is editable and remembered across restarts. */
    private void coinMultiplierRow(LinearLayout parent) {
        LinearLayout row = new LinearLayout(activity);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);

        TextView label = new TextView(activity);
        label.setText("Coins ×");
        label.setTextColor(Color.WHITE);
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        row.addView(label);

        int saved = prefs().getInt(PREF_COIN_MULTIPLIER, DEFAULT_COIN_MULTIPLIER);
        NativeBridge.setValue(NativeBridge.COIN_MULTIPLIER, saved);

        EditText factor = new EditText(activity);
        factor.setInputType(InputType.TYPE_CLASS_NUMBER);
        factor.setFilters(new InputFilter[] {new InputFilter.LengthFilter(4)});
        factor.setText(String.valueOf(saved));
        factor.setTextColor(Color.WHITE);
        factor.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        factor.setBackgroundTintList(ColorStateList.valueOf(ACCENT));
        factor.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        factor.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override public void afterTextChanged(Editable s) {
                int value;
                try {
                    value = Integer.parseInt(s.toString().trim());
                } catch (NumberFormatException e) {
                    return;  // empty while typing: keep the previous factor
                }
                if (value < 1) return;
                NativeBridge.setValue(NativeBridge.COIN_MULTIPLIER, value);
                prefs().edit().putInt(PREF_COIN_MULTIPLIER, value).apply();
            }
        });
        LinearLayout.LayoutParams factorLp = new LinearLayout.LayoutParams(dp(72), ViewGroup.LayoutParams.WRAP_CONTENT);
        factorLp.leftMargin = dp(6);
        row.addView(factor, factorLp);

        Switch toggle = featureSwitch("", NativeBridge.COIN_MULTIPLIER);
        // Weight 1 + MATCH width pushes the switch to the right edge like the other switches.
        row.addView(toggle, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        parent.addView(row);
    }

    /** An on/off switch that turns a native feature on and off. */
    private Switch featureSwitch(String text, int featureId) {
        Switch s = new Switch(activity);
        s.setText(text);
        s.setTextColor(Color.WHITE);
        s.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        s.setPadding(0, dp(8), 0, dp(12));
        // The game's activity uses the old Holo theme (square switch with ON/OFF text), so draw our own.
        s.setShowText(false);
        s.setSwitchMinWidth(dp(44));
        GradientDrawable thumb = new GradientDrawable();
        thumb.setShape(GradientDrawable.OVAL);
        thumb.setSize(dp(22), dp(22));
        thumb.setColor(Color.WHITE);
        GradientDrawable track = new GradientDrawable();
        track.setCornerRadius(dp(11));
        track.setSize(dp(44), dp(22));
        track.setColor(Color.WHITE);
        s.setThumbDrawable(thumb);
        s.setTrackDrawable(track);
        int[][] states = {{android.R.attr.state_checked}, {}};
        s.setThumbTintList(new ColorStateList(states, new int[] {Color.WHITE, Color.LTGRAY}));
        s.setTrackTintList(new ColorStateList(states, new int[] {ACCENT, Color.DKGRAY}));
        s.setOnCheckedChangeListener((button, checked) -> NativeBridge.set(featureId, checked));
        return s;
    }

    /** "Coins [ amount ] [Add]" plus a status line below it. */
    private void addCoinsRow(LinearLayout parent) {
        LinearLayout row = new LinearLayout(activity);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);

        TextView label = new TextView(activity);
        label.setText("Coins");
        label.setTextColor(Color.WHITE);
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        row.addView(label);

        EditText amount = new EditText(activity);
        amount.setInputType(InputType.TYPE_CLASS_NUMBER);
        amount.setFilters(new InputFilter[] {new InputFilter.LengthFilter(9)});
        amount.setHint("amount");
        amount.setHintTextColor(Color.GRAY);
        amount.setTextColor(Color.WHITE);
        amount.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        amount.setBackgroundTintList(ColorStateList.valueOf(ACCENT));
        amount.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        LinearLayout.LayoutParams amountLp = new LinearLayout.LayoutParams(dp(110), ViewGroup.LayoutParams.WRAP_CONTENT);
        amountLp.leftMargin = dp(12);
        amountLp.rightMargin = dp(8);
        row.addView(amount, amountLp);

        TextView add = new TextView(activity);
        add.setText("Add");
        add.setTextColor(Color.WHITE);
        add.setTypeface(Typeface.DEFAULT_BOLD);
        add.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        add.setPadding(dp(14), dp(6), dp(14), dp(6));
        add.setSingleLine(true);
        add.setGravity(Gravity.CENTER);
        GradientDrawable addBg = new GradientDrawable();
        addBg.setCornerRadius(dp(6));
        addBg.setColor(ACCENT);
        add.setBackground(addBg);
        row.addView(add, new LinearLayout.LayoutParams(dp(64), ViewGroup.LayoutParams.WRAP_CONTENT));
        // WRAP_CONTENT on purpose: a vertical LinearLayout ignores MATCH_PARENT children (its
        // default) when sizing itself, which squeezed this row to the slider's width.
        LinearLayout.LayoutParams rowLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rowLp.bottomMargin = dp(8);
        parent.addView(row, rowLp);

        // Hidden until there is a message, so it doesn't leave an empty gap.
        TextView status = new TextView(activity);
        status.setTextColor(Color.LTGRAY);
        status.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        status.setPadding(0, dp(2), 0, dp(6));
        status.setVisibility(View.GONE);
        parent.addView(status);

        add.setOnClickListener(v -> {
            hideKeyboard(amount);
            status.setVisibility(View.VISIBLE);
            int value;
            try {
                value = Integer.parseInt(amount.getText().toString().trim());
            } catch (NumberFormatException e) {
                status.setText("Enter a number first.");
                return;
            }
            if (value <= 0) {
                status.setText("Enter a number above 0.");
                return;
            }
            switch (NativeBridge.queueCoins(value)) {
                case NativeBridge.ADDED_RUN:
                    status.setText("Added " + value + " coins (run).");
                    break;
                case NativeBridge.ADDED_VILLAGE:
                    status.setText("Added " + value + " coins (village).");
                    break;
                default:
                    status.setText("Not available right now.");
                    break;
            }
        });
    }

    private void hideKeyboard(View view) {
        InputMethodManager imm = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
        view.clearFocus();
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
