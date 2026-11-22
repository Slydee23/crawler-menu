package crawlermenu;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

/**
 * The in-game menu: a draggable round button that opens a compact panel with a title bar, an
 * icon sidebar (Combat, Economy, Run, Settings) and a scrollable page per category.
 * Views are added to the activity's decor view, so no overlay permission is needed.
 * Built in code because the patched APK has no resources of ours.
 */
final class FloatingMenu {
    private static final String BUTTON_TAG = "crawlermenu.button";

    private static final String PREFS = "crawlermenu";
    private static final String PREF_PANEL_OPACITY = "panel_opacity";
    private static final String PREF_LAST_TAB = "last_tab";
    private static final String PREF_COIN_MULTIPLIER = "coin_multiplier";
    private static final String PREF_XP_MULTIPLIER = "xp_multiplier";
    private static final int DEFAULT_PANEL_OPACITY = 92;
    private static final int MIN_PANEL_OPACITY = 20;

    private static final int PANEL_WIDTH_DP = 380;
    private static final int PANEL_HEIGHT_DP = 240;
    private static final int SIDEBAR_WIDTH_DP = 64;

    private final Activity activity;
    private final ViewGroup decor;
    private final Theme theme;
    private final SharedPreferences prefs;
    private final Widgets widgets;

    private final List<Tab> tabs = new ArrayList<>();
    private GradientDrawable panelFill;
    private final LinearLayout panel;
    private final TextView button;

    /** One sidebar entry and its page. */
    private static final class Tab {
        final View item;
        final Icons.Icon icon;
        final TextView caption;
        final View page;

        Tab(View item, Icons.Icon icon, TextView caption, View page) {
            this.item = item;
            this.icon = icon;
            this.caption = caption;
            this.page = page;
        }
    }

    static void attach(Activity activity) {
        ViewGroup decor = (ViewGroup) activity.getWindow().getDecorView();
        if (decor.findViewWithTag(BUTTON_TAG) != null) return;  // already attached
        new FloatingMenu(activity, decor);
        Log.i(Loader.TAG, "Menu attached to " + activity.getClass().getName());
    }

    private FloatingMenu(Activity activity, ViewGroup decor) {
        this.activity = activity;
        this.decor = decor;
        this.theme = new Theme(activity);
        this.prefs = activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        this.widgets = new Widgets(activity, theme, prefs);

        panel = createPanel();
        button = createButton();
        panel.setVisibility(View.GONE);
        decor.addView(panel);
        decor.addView(button);
    }

    private int dp(float v) {
        return theme.dp(v);
    }

    // --- Floating button -----------------------------------------------------------------------

    private TextView createButton() {
        TextView b = theme.heading("CM", 14, Theme.TEXT);
        b.setTag(BUTTON_TAG);
        b.setGravity(Gravity.CENTER);
        b.setLetterSpacing(0.05f);
        b.setBackground(theme.buttonDisc());

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(dp(48), dp(48));
        lp.gravity = Gravity.TOP | Gravity.START;
        lp.leftMargin = dp(16);
        lp.topMargin = dp(96);
        b.setLayoutParams(lp);
        b.setOnTouchListener(new Dragger(b, this::togglePanel));
        return b;
    }

    private void togglePanel() {
        panel.setVisibility(panel.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
        button.bringToFront();
    }

    // --- Panel ---------------------------------------------------------------------------------

    private LinearLayout createPanel() {
        LinearLayout p = new LinearLayout(activity);
        p.setOrientation(LinearLayout.VERTICAL);
        GradientDrawable[] fill = new GradientDrawable[1];
        p.setBackground(theme.panelFrame(fill));
        panelFill = fill[0];
        setPanelOpacity(prefs.getInt(PREF_PANEL_OPACITY, DEFAULT_PANEL_OPACITY));
        int inset = dp(5);  // keep content inside the double gold frame
        p.setPadding(inset, inset, inset, inset);
        p.setClickable(true);  // taps on the panel must not reach the game

        p.addView(createTitleBar(p));
        p.addView(line(ViewGroup.LayoutParams.MATCH_PARENT, 1));

        LinearLayout body = new LinearLayout(activity);
        body.setOrientation(LinearLayout.HORIZONTAL);

        LinearLayout pages = new LinearLayout(activity);
        pages.setOrientation(LinearLayout.VERTICAL);
        pages.setPadding(dp(14), dp(8), dp(14), dp(8));
        ScrollView scroll = new ScrollView(activity);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.addView(pages);

        body.addView(createSidebar(pages));
        body.addView(line(1, ViewGroup.LayoutParams.MATCH_PARENT));
        body.addView(scroll, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f));
        p.addView(body, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        selectTab(Math.max(0, Math.min(prefs.getInt(PREF_LAST_TAB, 0), tabs.size() - 1)));

        // Fixed compact size, but never larger than the screen.
        DisplayMetrics dm = activity.getResources().getDisplayMetrics();
        int width = Math.min(dp(PANEL_WIDTH_DP), Math.round(dm.widthPixels * 0.9f));
        int height = Math.min(dp(PANEL_HEIGHT_DP), Math.round(dm.heightPixels * 0.85f));
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(width, height);
        lp.gravity = Gravity.CENTER;
        p.setLayoutParams(lp);
        return p;
    }

    /** "CRAWLER MENU ............ ✕" — also the drag handle for the panel. */
    private View createTitleBar(LinearLayout panelView) {
        LinearLayout bar = new LinearLayout(activity);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setPadding(dp(12), dp(6), dp(6), dp(6));

        TextView title = theme.heading("Crawler Menu", 13, Theme.GOLD);
        bar.addView(title, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        TextView close = theme.body("✕", 15, Theme.TEXT_MUTED);
        close.setPadding(dp(10), dp(2), dp(8), dp(2));
        close.setOnClickListener(v -> panelView.setVisibility(View.GONE));
        bar.addView(close);

        bar.setOnTouchListener(new Dragger(panelView, null));
        return bar;
    }

    private View line(int width, int height) {
        View v = new View(activity);
        v.setBackground(theme.divider());
        v.setLayoutParams(new LinearLayout.LayoutParams(width, height));
        return v;
    }

    // --- Sidebar and pages ---------------------------------------------------------------------

    private View createSidebar(LinearLayout pages) {
        LinearLayout side = new LinearLayout(activity);
        side.setOrientation(LinearLayout.VERTICAL);
        side.setBackgroundColor(Color.argb(120, Color.red(Theme.SIDEBAR), Color.green(Theme.SIDEBAR),
                Color.blue(Theme.SIDEBAR)));
        side.setLayoutParams(new LinearLayout.LayoutParams(dp(SIDEBAR_WIDTH_DP), ViewGroup.LayoutParams.MATCH_PARENT));

        addTab(side, pages, Icons.Kind.SWORDS, "Combat", combatPage());
        addTab(side, pages, Icons.Kind.COIN, "Economy", economyPage());
        addTab(side, pages, Icons.Kind.SKULL, "Run", runPage());
        side.addView(new View(activity), new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
        addTab(side, pages, Icons.Kind.GEAR, "Settings", settingsPage());
        return side;
    }

    private void addTab(LinearLayout side, LinearLayout pages, Icons.Kind kind, String name, View page) {
        LinearLayout item = new LinearLayout(activity);
        item.setOrientation(LinearLayout.VERTICAL);
        item.setGravity(Gravity.CENTER);
        item.setPadding(0, dp(7), 0, dp(7));

        Icons.Icon icon = Icons.create(kind, Theme.TEXT_MUTED);
        ImageView iconView = new ImageView(activity);
        iconView.setImageDrawable(icon);
        LinearLayout.LayoutParams iconLp = new LinearLayout.LayoutParams(dp(22), dp(22));
        iconLp.bottomMargin = dp(3);
        item.addView(iconView, iconLp);
        TextView caption = theme.heading(name, 7.5f, Theme.TEXT_MUTED);
        caption.setGravity(Gravity.CENTER);
        caption.setLetterSpacing(0.08f);
        item.addView(caption);

        side.addView(item, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        page.setVisibility(View.GONE);
        pages.addView(page);

        int index = tabs.size();
        tabs.add(new Tab(item, icon, caption, page));
        item.setOnClickListener(v -> selectTab(index));
    }

    private void selectTab(int index) {
        for (int i = 0; i < tabs.size(); i++) {
            Tab t = tabs.get(i);
            boolean active = i == index;
            t.page.setVisibility(active ? View.VISIBLE : View.GONE);
            t.item.setBackground(active ? activeTabBackground() : null);
            t.icon.setColor(active ? Theme.GOLD : Theme.TEXT_MUTED);
            t.caption.setTextColor(active ? Theme.TEXT : Theme.TEXT_MUTED);
        }
        prefs.edit().putInt(PREF_LAST_TAB, index).apply();
    }

    /** Dark red tint with a blood red bar on the left edge. */
    private Drawable activeTabBackground() {
        GradientDrawable tint = new GradientDrawable();
        tint.setColor(Color.argb(150, Color.red(Theme.BLOOD_DARK), Color.green(Theme.BLOOD_DARK),
                Color.blue(Theme.BLOOD_DARK)));
        GradientDrawable bar = new GradientDrawable();
        bar.setColor(Theme.BLOOD);
        LayerDrawable bg = new LayerDrawable(new Drawable[] {tint, bar});
        bg.setLayerInset(1, 0, 0, dp(SIDEBAR_WIDTH_DP) - dp(3), 0);
        return bg;
    }

    /** Page container with a "— TITLE —" header. */
    private LinearLayout page(String title) {
        LinearLayout p = new LinearLayout(activity);
        p.setOrientation(LinearLayout.VERTICAL);
        TextView header = theme.heading("— " + title + " —", 12, Theme.GOLD);
        header.setPadding(0, 0, 0, dp(4));
        p.addView(header);
        return p;
    }

    private View combatPage() {
        LinearLayout p = page("Combat");
        p.addView(widgets.switchRow("Godmode", NativeBridge.GODMODE));
        p.addView(widgets.switchRow("One-hit kill", NativeBridge.ONE_HIT_KILL));
        p.addView(widgets.switchRow("No mana cost", NativeBridge.NO_MANA_COST));
        return p;
    }

    private View economyPage() {
        LinearLayout p = page("Economy");
        p.addView(widgets.multiplierRow("Coins", NativeBridge.COIN_MULTIPLIER, PREF_COIN_MULTIPLIER));
        p.addView(widgets.multiplierRow("XP", NativeBridge.XP_MULTIPLIER, PREF_XP_MULTIPLIER));
        p.addView(widgets.addCoinsRow());
        return p;
    }

    private View runPage() {
        LinearLayout p = page("Run");
        p.addView(widgets.switchRow("Combo never breaks", NativeBridge.COMBO_NEVER_BREAKS));
        p.addView(widgets.switchRow("Infinite rerolls, skips & banishes", NativeBridge.INFINITE_CHOICES));
        p.addView(widgets.runStatsSection());
        return p;
    }

    private View settingsPage() {
        LinearLayout p = page("Settings");
        int opacity = prefs.getInt(PREF_PANEL_OPACITY, DEFAULT_PANEL_OPACITY);
        p.addView(widgets.percentSlider("Background opacity", MIN_PANEL_OPACITY, 100, opacity,
                this::setPanelOpacity, value -> prefs.edit().putInt(PREF_PANEL_OPACITY, value).apply()));
        TextView note = theme.body("Cheats are off every time the game starts.", 12, Theme.TEXT_MUTED);
        note.setPadding(0, dp(6), 0, 0);
        p.addView(note);
        return p;
    }

    /** Sets the panel background's fill alpha; {@code percent} is 0..100. Text stays opaque. */
    private void setPanelOpacity(int percent) {
        if (panelFill == null) return;
        panelFill.setColor(Color.argb(Math.round(percent * 2.55f), Color.red(Theme.BACKGROUND),
                Color.green(Theme.BACKGROUND), Color.blue(Theme.BACKGROUND)));
    }

    // --- Dragging ------------------------------------------------------------------------------

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
