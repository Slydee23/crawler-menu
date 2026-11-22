package crawlermenu;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;

/**
 * Sidebar icons drawn as vector line art on a 24×24 grid, so they look the same on every
 * device (font glyphs like ⚔ or ☠ depend on the system font).
 */
final class Icons {
    enum Kind { SWORDS, COIN, SKULL, GEAR }

    private Icons() {}

    static Icon create(Kind kind, int color) {
        return new Icon(kind, color);
    }

    /** A line-art icon; {@link #setColor} recolors it (e.g. gold when the tab is active). */
    static final class Icon extends Drawable {
        private static final float GRID = 24f;
        private static final float STROKE = 1.8f;  // in grid units

        private final Kind kind;
        private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path path = new Path();

        Icon(Kind kind, int color) {
            this.kind = kind;
            stroke.setStyle(Paint.Style.STROKE);
            stroke.setStrokeWidth(STROKE);
            stroke.setStrokeCap(Paint.Cap.ROUND);
            stroke.setStrokeJoin(Paint.Join.ROUND);
            fill.setStyle(Paint.Style.FILL);
            setColor(color);
        }

        void setColor(int color) {
            stroke.setColor(color);
            fill.setColor(color);
            invalidateSelf();
        }

        @Override public void draw(Canvas canvas) {
            Rect b = getBounds();
            float scale = Math.min(b.width(), b.height()) / GRID;
            canvas.save();
            canvas.translate(b.left + (b.width() - GRID * scale) / 2f, b.top + (b.height() - GRID * scale) / 2f);
            canvas.scale(scale, scale);
            switch (kind) {
                case SWORDS: drawSwords(canvas); break;
                case COIN: drawCoin(canvas); break;
                case SKULL: drawSkull(canvas); break;
                case GEAR: drawGear(canvas); break;
            }
            canvas.restore();
        }

        /** Two crossed swords: blades, cross guards, grips and pommels. */
        private void drawSwords(Canvas c) {
            // Sword 1: tip top-right, grip bottom-left.
            c.drawLine(7f, 17f, 19.5f, 4.5f, stroke);     // blade
            c.drawLine(5.5f, 14.5f, 9.5f, 18.5f, stroke); // guard
            c.drawLine(7f, 17f, 4.5f, 19.5f, stroke);     // grip
            c.drawCircle(4f, 20f, 1.1f, fill);            // pommel
            // Sword 2: mirrored.
            c.drawLine(17f, 17f, 4.5f, 4.5f, stroke);
            c.drawLine(18.5f, 14.5f, 14.5f, 18.5f, stroke);
            c.drawLine(17f, 17f, 19.5f, 19.5f, stroke);
            c.drawCircle(20f, 20f, 1.1f, fill);
        }

        /** A coin: outer rim, inner ring and a small diamond in the middle. */
        private void drawCoin(Canvas c) {
            c.drawCircle(12f, 12f, 8.5f, stroke);
            c.drawCircle(12f, 12f, 5.5f, stroke);
            path.reset();
            path.moveTo(12f, 9.6f);
            path.lineTo(14.4f, 12f);
            path.lineTo(12f, 14.4f);
            path.lineTo(9.6f, 12f);
            path.close();
            c.drawPath(path, fill);
        }

        /** A skull: rounded cranium, jaw, eye sockets, nose and teeth. */
        private void drawSkull(Canvas c) {
            path.reset();
            // Cranium arc over the top, open at the bottom where the jaw starts.
            path.arcTo(new RectF(4.5f, 2.5f, 19.5f, 17.5f), 125f, 290f, true);
            path.lineTo(15.5f, 20.5f);
            path.lineTo(8.5f, 20.5f);
            path.close();
            c.drawPath(path, stroke);
            c.drawCircle(9.2f, 10.5f, 1.9f, fill);   // eyes
            c.drawCircle(14.8f, 10.5f, 1.9f, fill);
            path.reset();                            // nose
            path.moveTo(12f, 12.8f);
            path.lineTo(13.1f, 14.8f);
            path.lineTo(10.9f, 14.8f);
            path.close();
            c.drawPath(path, fill);
            c.drawLine(11f, 17.5f, 11f, 20.5f, stroke);  // teeth
            c.drawLine(13f, 17.5f, 13f, 20.5f, stroke);
        }

        /** A gear: eight teeth around a ring with a hole. */
        private void drawGear(Canvas c) {
            Paint teeth = new Paint(stroke);
            teeth.setStrokeWidth(2.6f);
            teeth.setStrokeCap(Paint.Cap.BUTT);
            for (int i = 0; i < 8; i++) {
                double a = Math.toRadians(i * 45.0);
                float cos = (float) Math.cos(a);
                float sin = (float) Math.sin(a);
                c.drawLine(12f + cos * 6.5f, 12f + sin * 6.5f, 12f + cos * 9.6f, 12f + sin * 9.6f, teeth);
            }
            c.drawCircle(12f, 12f, 6.5f, stroke);
            c.drawCircle(12f, 12f, 2.6f, stroke);
        }

        @Override public void setAlpha(int alpha) {
            stroke.setAlpha(alpha);
            fill.setAlpha(alpha);
        }

        @Override public void setColorFilter(ColorFilter cf) {
            stroke.setColorFilter(cf);
            fill.setColorFilter(cf);
        }

        @Override public int getOpacity() {
            return PixelFormat.TRANSLUCENT;
        }
    }
}
