package crawlermenu;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

/** The building blocks of the menu pages: switches, multiplier rows, the add-coins row, sliders. */
final class Widgets {
    static final int DEFAULT_MULTIPLIER = 10;

    private final Activity activity;
    private final Theme theme;
    private final SharedPreferences prefs;

    Widgets(Activity activity, Theme theme, SharedPreferences prefs) {
        this.activity = activity;
        this.theme = theme;
        this.prefs = prefs;
    }

    private int dp(float v) {
        return theme.dp(v);
    }

    /** A horizontal row that is as wide as its page and vertically centered. */
    private LinearLayout row() {
        LinearLayout row = new LinearLayout(activity);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setMinimumHeight(dp(40));
        row.setLayoutParams(new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        return row;
    }

    private static LinearLayout.LayoutParams fill() {
        return new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
    }

    /** Pill-shaped switch: blood red track when on, dark when off. */
    Switch toggle(int featureId) {
        Switch s = new Switch(activity);
        s.setShowText(false);  // the game's Holo theme would print ON/OFF otherwise
        s.setSwitchMinWidth(dp(40));
        GradientDrawable thumb = new GradientDrawable();
        thumb.setShape(GradientDrawable.OVAL);
        thumb.setSize(dp(20), dp(20));
        thumb.setColor(Color.WHITE);
        GradientDrawable track = new GradientDrawable();
        track.setCornerRadius(dp(10));
        track.setSize(dp(40), dp(20));
        track.setColor(Color.WHITE);
        s.setThumbDrawable(thumb);
        s.setTrackDrawable(track);
        int[][] states = {{android.R.attr.state_checked}, {}};
        s.setThumbTintList(new ColorStateList(states, new int[] {Theme.TEXT, Theme.TEXT_MUTED}));
        s.setTrackTintList(new ColorStateList(states, new int[] {Theme.BLOOD, Theme.TRACK_OFF}));
        s.setOnCheckedChangeListener((button, checked) -> NativeBridge.set(featureId, checked));
        return s;
    }

    /** "Label ............ (switch)" */
    View switchRow(String label, int featureId) {
        LinearLayout row = row();
        TextView text = theme.body(label, 14, Theme.TEXT);
        text.setPadding(0, dp(4), dp(8), dp(4));
        row.addView(text, fill());
        row.addView(toggle(featureId));
        return row;
    }

    private EditText numberField(int maxDigits, String hint) {
        EditText field = new EditText(activity);
        field.setInputType(InputType.TYPE_CLASS_NUMBER);
        field.setFilters(new InputFilter[] {new InputFilter.LengthFilter(maxDigits)});
        field.setHint(hint);
        field.setHintTextColor(Theme.TEXT_MUTED);
        field.setTextColor(Theme.TEXT);
        field.setTypeface(Theme.BODY_BOLD);
        field.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        field.setGravity(Gravity.CENTER);
        field.setBackgroundTintList(theme.underlineTint());
        field.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        return field;
    }

    /**
     * "Coins ×  [ 10 ]  ...... (switch)" — the factor is editable and remembered (under
     * {@code prefKey}); the switch is off on every game start like all cheats.
     */
    View multiplierRow(String name, int featureId, String prefKey) {
        LinearLayout row = row();
        TextView label = theme.body(name + " ×", 14, Theme.TEXT);
        row.addView(label);

        int saved = prefs.getInt(prefKey, DEFAULT_MULTIPLIER);
        NativeBridge.setValue(featureId, saved);

        EditText factor = numberField(4, "");
        factor.setText(String.valueOf(saved));
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
                NativeBridge.setValue(featureId, value);
                prefs.edit().putInt(prefKey, value).apply();
            }
        });
        LinearLayout.LayoutParams factorLp = new LinearLayout.LayoutParams(dp(64), ViewGroup.LayoutParams.WRAP_CONTENT);
        factorLp.leftMargin = dp(6);
        row.addView(factor, factorLp);

        row.addView(new View(activity), fill());  // pushes the switch to the right edge
        row.addView(toggle(featureId));
        return row;
    }

    /** "Add coins [ amount ] [ADD]" with a status line below (hidden until used). */
    View addCoinsRow() {
        LinearLayout box = new LinearLayout(activity);
        box.setOrientation(LinearLayout.VERTICAL);

        LinearLayout row = row();
        row.addView(theme.body("Add coins", 14, Theme.TEXT));

        EditText amount = numberField(9, "amount");
        LinearLayout.LayoutParams amountLp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        amountLp.leftMargin = dp(8);
        amountLp.rightMargin = dp(8);
        row.addView(amount, amountLp);

        TextView add = theme.heading("Add", 12, Theme.TEXT);
        add.setGravity(Gravity.CENTER);
        add.setSingleLine(true);
        add.setPadding(dp(12), dp(6), dp(12), dp(6));
        add.setBackground(theme.actionButton());
        row.addView(add);
        box.addView(row);

        TextView status = theme.body("", 12, Theme.TEXT_MUTED);
        status.setPadding(0, 0, 0, dp(4));
        status.setVisibility(View.GONE);
        box.addView(status);

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
        return box;
    }

    /** Labeled slider, value range [min, max] in percent. */
    View percentSlider(String label, int min, int max, int value, IntConsumer onChange, IntConsumer onCommit) {
        LinearLayout box = new LinearLayout(activity);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(0, dp(6), 0, dp(6));

        TextView text = theme.body(label + ": " + value + "%", 13, Theme.TEXT);
        box.addView(text);

        SeekBar slider = new SeekBar(activity);
        slider.setMax(max - min);
        slider.setProgress(value - min);
        slider.setProgressTintList(theme.accentTint());
        slider.setThumbTintList(ColorStateList.valueOf(Theme.GOLD));
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int progress, boolean fromUser) {
                text.setText(label + ": " + (progress + min) + "%");
                onChange.accept(progress + min);
            }

            @Override public void onStartTrackingTouch(SeekBar s) {}

            @Override public void onStopTrackingTouch(SeekBar s) {
                onCommit.accept(s.getProgress() + min);
            }
        });
        box.addView(slider, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        return box;
    }

    private void hideKeyboard(View view) {
        InputMethodManager imm = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
        view.clearFocus();
    }

    /** Minimal int callback (java.util.function needs API 24; keep it explicit). */
    interface IntConsumer {
        void accept(int value);
    }
}
