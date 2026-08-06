package com.lordofrobots.telemetry;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Shader;
import android.view.View;

import java.util.ArrayDeque;
import java.util.ArrayList;

final class BatteryGraphView extends View {
    private static final long WINDOW_MS = 30_000;
    private static final float MIN_V = 9.75f;
    private static final float MAX_V = 13.15f;
    private static final int GREEN = 0xff45d483;
    private static final int YELLOW = 0xffffd34e;
    private static final int RED = 0xffff5964;
    private final ArrayDeque<Sample> samples = new ArrayDeque<>();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path line = new Path();
    private final Path fill = new Path();

    BatteryGraphView(Context context) { super(context); }

    void addSample(float volts, long now) {
        if (volts < 1.0f || volts > 16.0f) return;
        samples.addLast(new Sample(now, volts));
        while (!samples.isEmpty() && now - samples.peekFirst().time > WINDOW_MS) {
            samples.removeFirst();
        }
        invalidate();
    }

    void clearSamples() {
        samples.clear();
        invalidate();
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float w = getWidth(), h = getHeight();
        float left = dp(31), right = w - dp(7), top = dp(3), bottom = h - dp(12);

        // Very subtle condition field: green at full, yellow around half, red low.
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(new LinearGradient(0, top, 0, bottom,
                new int[]{0x1645d483, 0x12ffd34e, 0x16ff5964},
                new float[]{0f, .50f, 1f}, Shader.TileMode.CLAMP));
        canvas.drawRoundRect(left, top, right, bottom, dp(7), dp(7), paint);
        paint.setShader(null);

        paint.setStrokeWidth(dp(.7f));
        paint.setTextSize(dp(7));
        paint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        float[] levels = {13.05f, 11.48f, 9.90f};
        String[] labels = {"13.1", "11.5", "9.9"};
        for (int i = 0; i < levels.length; i++) {
            float y = yFor(levels[i], top, bottom);
            paint.setColor(i == 0 ? 0x3045d483 : (i == 1 ? 0x30ffd34e : 0x40ff5964));
            canvas.drawLine(left, y, right, y, paint);
            paint.setColor(0xff8296b6);
            canvas.drawText(labels[i], dp(2), y + dp(2.5f), paint);
        }

        paint.setColor(0xff8296b6);
        canvas.drawText("-30s", left, h - dp(1), paint);
        paint.setTextAlign(Paint.Align.RIGHT);
        canvas.drawText("NOW", right, h - dp(1), paint);
        paint.setTextAlign(Paint.Align.LEFT);

        if (samples.size() < 2) {
            paint.setColor(0xff8296b6);
            paint.setTextSize(dp(9));
            paint.setTextAlign(Paint.Align.CENTER);
            canvas.drawText("COLLECTING 30 SEC HISTORY", (left + right) / 2,
                    (top + bottom) / 2 + dp(3), paint);
            paint.setTextAlign(Paint.Align.LEFT);
            return;
        }

        long now = samples.peekLast().time;
        ArrayList<Float> xs = new ArrayList<>();
        ArrayList<Float> ys = new ArrayList<>();
        for (Sample sample : samples) {
            xs.add(right - ((now - sample.time) / (float) WINDOW_MS) * (right - left));
            ys.add(yFor(sample.volts, top, bottom));
        }

        line.reset();
        fill.reset();
        line.moveTo(xs.get(0), ys.get(0));
        fill.moveTo(xs.get(0), bottom);
        fill.lineTo(xs.get(0), ys.get(0));
        for (int i = 1; i < xs.size(); i++) {
            float previousX = xs.get(i - 1), previousY = ys.get(i - 1);
            float midpointX = (previousX + xs.get(i)) / 2f;
            float midpointY = (previousY + ys.get(i)) / 2f;
            line.quadTo(previousX, previousY, midpointX, midpointY);
            fill.quadTo(previousX, previousY, midpointX, midpointY);
        }
        int last = xs.size() - 1;
        line.lineTo(xs.get(last), ys.get(last));
        fill.lineTo(xs.get(last), ys.get(last));
        fill.lineTo(xs.get(last), bottom);
        fill.close();

        float currentVolts = samples.peekLast().volts;
        int conditionColor = conditionColor(percentFor(currentVolts));
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(new LinearGradient(0, top, 0, bottom,
                withAlpha(conditionColor, 0x58), withAlpha(conditionColor, 0x08),
                Shader.TileMode.CLAMP));
        canvas.drawPath(fill, paint);
        paint.setShader(null);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(dp(2.2f));
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setColor(conditionColor);
        canvas.drawPath(line, paint);
        paint.setStyle(Paint.Style.FILL);
        canvas.drawCircle(xs.get(last), ys.get(last), dp(3.2f), paint);
    }

    static int conditionColor(int percent) {
        int p = Math.max(0, Math.min(100, percent));
        if (p <= 25) return RED;
        if (p <= 50) return blend(RED, YELLOW, (p - 25) / 25f);
        return blend(YELLOW, GREEN, (p - 50) / 50f);
    }

    private static int percentFor(float volts) {
        return Math.round(100f * (volts - 9.90f) / (13.05f - 9.90f));
    }

    private static int blend(int from, int to, float amount) {
        float t = Math.max(0f, Math.min(1f, amount));
        return Color.rgb(
                Math.round(Color.red(from) + (Color.red(to) - Color.red(from)) * t),
                Math.round(Color.green(from) + (Color.green(to) - Color.green(from)) * t),
                Math.round(Color.blue(from) + (Color.blue(to) - Color.blue(from)) * t));
    }

    private static int withAlpha(int color, int alpha) {
        return Color.argb(alpha, Color.red(color), Color.green(color), Color.blue(color));
    }

    private float yFor(float volts, float top, float bottom) {
        float clamped = Math.max(MIN_V, Math.min(MAX_V, volts));
        return bottom - (clamped - MIN_V) / (MAX_V - MIN_V) * (bottom - top);
    }

    private float dp(float value) {
        float density = getResources().getDisplayMetrics().density;
        float widthDp = getWidth() > 0
                ? getWidth() / density
                : getResources().getDisplayMetrics().widthPixels / density;
        float scale = Math.max(1f, Math.min(1.35f, widthDp / 400f));
        return value * density * scale;
    }

    private static final class Sample {
        final long time;
        final float volts;
        Sample(long time, float volts) { this.time = time; this.volts = volts; }
    }
}
