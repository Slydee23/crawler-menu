package crawlermenu;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.util.TypedValue;
import android.widget.TextView;

/** "Gothic gaming" look: near-black panels, thin gold ornaments, blood red for active states. */
final class Theme {
    static final int BACKGROUND = Color.rgb(0x0E, 0x0A, 0x0C);   // panel fill (alpha set by the opacity slider)
    static final int SIDEBAR = Color.rgb(0x07, 0x05, 0x06);
    static final int GOLD = Color.rgb(0xC9, 0xA2, 0x4A);
    static final int GOLD_DIM = Color.argb(0x66, 0xC9, 0xA2, 0x4A);
    static final int BLOOD = Color.rgb(0x9E, 0x10, 0x22);
    static final int BLOOD_DARK = Color.rgb(0x4A, 0x08, 0x12);
    static final int TEXT = Color.rgb(0xEC, 0xE3, 0xD0);          // parchment
    static final int TEXT_MUTED = Color.rgb(0x8C, 0x82, 0x79);
    static final int TRACK_OFF = Color.rgb(0x2A, 0x24, 0x26);

    static final Typeface HEADING = Typeface.create(Typeface.SERIF, Typeface.BOLD);
    static final Typeface BODY = Typeface.create("sans-serif", Typeface.NORMAL);
    static final Typeface BODY_BOLD = Typeface.create("sans-serif", Typeface.BOLD);

    private final Context context;

    Theme(Context context) {
        this.context = context;
    }

    int dp(float value) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value,
                context.getResources().getDisplayMetrics()));
    }

    /** Uppercase serif heading with letter spacing. */
    TextView heading(String text, float sizeSp, int color) {
        TextView t = new TextView(context);
        t.setText(text.toUpperCase());
        t.setTypeface(HEADING);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sizeSp);
        t.setTextColor(color);
        t.setLetterSpacing(0.12f);
        return t;
    }

    TextView body(String text, float sizeSp, int color) {
        TextView t = new TextView(context);
        t.setText(text);
        t.setTypeface(BODY);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sizeSp);
        t.setTextColor(color);
        return t;
    }

    /**
     * Panel frame: gold outer line, a thin inner gold line inset by 3dp, and the (translucent) fill.
     * The fill drawable is returned via {@code fillOut[0]} so the opacity slider can change it.
     */
    Drawable panelFrame(GradientDrawable[] fillOut) {
        float r = dp(6);
        GradientDrawable outer = new GradientDrawable();
        outer.setCornerRadius(r);
        outer.setColor(Color.TRANSPARENT);
        outer.setStroke(Math.max(1, dp(1.5f)), GOLD);

        GradientDrawable fill = new GradientDrawable();
        fill.setCornerRadius(r);
        fill.setColor(BACKGROUND);

        GradientDrawable inner = new GradientDrawable();
        inner.setCornerRadius(dp(3));
        inner.setColor(Color.TRANSPARENT);
        inner.setStroke(1, GOLD_DIM);

        LayerDrawable frame = new LayerDrawable(new Drawable[] {fill, inner, outer});
        int inset = dp(4);
        frame.setLayerInset(1, inset, inset, inset, inset);
        if (fillOut != null) fillOut[0] = fill;
        return frame;
    }

    /** Round floating button: black disc, gold ring. */
    Drawable buttonDisc() {
        GradientDrawable d = new GradientDrawable();
        d.setShape(GradientDrawable.OVAL);
        d.setColor(Color.argb(230, 0x0E, 0x0A, 0x0C));
        d.setStroke(dp(2), GOLD);
        return d;
    }

    /** Blood red button with a thin gold edge. */
    Drawable actionButton() {
        GradientDrawable d = new GradientDrawable();
        d.setCornerRadius(dp(3));
        d.setColor(BLOOD);
        d.setStroke(1, GOLD);
        return d;
    }

    /** Thin gold divider line. */
    Drawable divider() {
        GradientDrawable d = new GradientDrawable();
        d.setColor(GOLD_DIM);
        return d;
    }

    ColorStateList accentTint() {
        return ColorStateList.valueOf(BLOOD);
    }

    ColorStateList underlineTint() {
        return ColorStateList.valueOf(GOLD_DIM);
    }
}
