package com.lordofrobots.telemetry;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.View;

import java.util.Locale;

final class GamepadTelemetryView extends View {
    private static final int CYAN = 0xff54dcff;
    private static final int ACTIVE = 0xffffd166;
    private final Bitmap image;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private ControllerTelemetryPacket packet;
    private boolean connected;

    GamepadTelemetryView(Context context) {
        super(context);
        image = BitmapFactory.decodeResource(getResources(), R.drawable.gamepad_telemetry);
    }

    void setPacket(ControllerTelemetryPacket packet) { this.packet = packet; invalidate(); }
    void setConnected(boolean connected) { this.connected = connected; invalidate(); }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float w = getWidth(), h = getHeight();
        RectF dst = fit(image.getWidth(), image.getHeight(), w, h);
        canvas.drawBitmap(image, null, dst, paint);
        connectionBadge(canvas, connected);
        if (packet == null) {
            badge(canvas, x(dst, .5f), y(dst, .90f), "WAITING FOR GAMEPAD DATA", 0xfff0b429);
            return;
        }

        stick(canvas, x(dst, .29f), y(dst, .34f), packet.axisX, packet.axisY, packet.button(1 << 8));
        stick(canvas, x(dst, .59f), y(dst, .53f), packet.axisRX, packet.axisRY, packet.button(1 << 9));
        button(canvas, x(dst, .69f), y(dst, .43f), packet.button(1 << 0), "A");
        button(canvas, x(dst, .75f), y(dst, .34f), packet.button(1 << 1), "B");
        button(canvas, x(dst, .64f), y(dst, .34f), packet.button(1 << 2), "X");
        button(canvas, x(dst, .69f), y(dst, .24f), packet.button(1 << 3), "Y");

        smallButton(canvas, x(dst, .43f), y(dst, .32f), packet.misc(1 << 1));
        smallButton(canvas, x(dst, .54f), y(dst, .32f), packet.misc(1 << 2));
        smallButton(canvas, x(dst, .49f), y(dst, .20f), packet.misc(1 << 0));
        smallButton(canvas, x(dst, .49f), y(dst, .39f), packet.misc(1 << 3));

        dpad(canvas, x(dst, .37f), y(dst, .53f), packet.dpad);
        trigger(canvas, x(dst, .27f), y(dst, .08f), packet.brake, "LT", packet.button(1 << 6));
        trigger(canvas, x(dst, .73f), y(dst, .08f), packet.throttle, "RT", packet.button(1 << 7));
        shoulder(canvas, x(dst, .36f), y(dst, .11f), packet.button(1 << 4), "LB");
        shoulder(canvas, x(dst, .64f), y(dst, .11f), packet.button(1 << 5), "RB");

        badge(canvas, x(dst, .28f), y(dst, .88f),
                String.format(Locale.US, "L  %+d / %+d", packet.axisX, packet.axisY), CYAN);
        badge(canvas, x(dst, .72f), y(dst, .88f),
                String.format(Locale.US, "R  %+d / %+d", packet.axisRX, packet.axisRY), CYAN);
        String battery = packet.battery == 0 ? "PAD BATTERY  N/A" :
                "PAD BATTERY  " + Math.round(packet.battery * 100f / 255f) + "%";
        badge(canvas, x(dst, .5f), y(dst, .97f), battery, 0xff8296b6);
    }

    private void stick(Canvas c, float x, float y, int ax, int ay, boolean pressed) {
        float travel = dp(17);
        float px = x + Math.max(-1, Math.min(1, ax / 512f)) * travel;
        float py = y + Math.max(-1, Math.min(1, ay / 512f)) * travel;
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(dp(2));
        paint.setColor(pressed ? ACTIVE : CYAN);
        c.drawCircle(x, y, dp(25), paint);
        c.drawLine(x, y, px, py, paint);
        paint.setStyle(Paint.Style.FILL);
        c.drawCircle(px, py, pressed ? dp(8) : dp(6), paint);
    }

    private void button(Canvas c, float x, float y, boolean active, String label) {
        if (!active) return;
        paint.setColor(0xccffd166);
        c.drawCircle(x, y, dp(22), paint);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        paint.setTextSize(dp(16));
        paint.setColor(0xff07152e);
        c.drawText(label, x, y + dp(6), paint);
    }

    private void smallButton(Canvas c, float x, float y, boolean active) {
        if (!active) return;
        paint.setColor(0xddffd166);
        c.drawCircle(x, y, dp(11), paint);
    }

    private void dpad(Canvas c, float x, float y, int dpad) {
        paint.setColor(0xddffd166);
        float d = dp(28), s = dp(11);
        if ((dpad & 1) != 0) c.drawRoundRect(x - s, y - d, x + s, y - s, s, s, paint);
        if ((dpad & 2) != 0) c.drawRoundRect(x - s, y + s, x + s, y + d, s, s, paint);
        if ((dpad & 4) != 0) c.drawRoundRect(x + s, y - s, x + d, y + s, s, s, paint);
        if ((dpad & 8) != 0) c.drawRoundRect(x - d, y - s, x - s, y + s, s, s, paint);
    }

    private void trigger(Canvas c, float x, float y, int value, String name, boolean digital) {
        float width = dp(70), height = dp(8);
        paint.setColor(0xaa07152e);
        c.drawRoundRect(x - width / 2, y, x + width / 2, y + height, height, height, paint);
        paint.setColor(digital ? ACTIVE : CYAN);
        float fill = width * Math.min(1f, value / 1023f);
        c.drawRoundRect(x - width / 2, y, x - width / 2 + fill, y + height, height, height, paint);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        paint.setTextSize(dp(10));
        paint.setColor(Color.WHITE);
        c.drawText(name + " " + value, x, y + dp(21), paint);
    }

    private void shoulder(Canvas c, float x, float y, boolean active, String name) {
        if (!active) return;
        badge(c, x, y, name, ACTIVE);
    }

    private void badge(Canvas c, float x, float y, String text, int color) {
        paint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        paint.setTextSize(dp(10));
        paint.setTextAlign(Paint.Align.CENTER);
        float width = paint.measureText(text) + dp(16);
        paint.setColor(0xdd07152e);
        c.drawRoundRect(x - width / 2, y - dp(13), x + width / 2, y + dp(6), dp(9), dp(9), paint);
        paint.setColor(color);
        c.drawText(text, x, y + dp(1), paint);
    }

    private void connectionBadge(Canvas canvas, boolean linked) {
        String text = linked ? "LINKED" : "WAITING";
        int color = linked ? 0xff45d483 : 0xfff0b429;
        paint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        paint.setTextSize(dp(8));
        paint.setTextAlign(Paint.Align.LEFT);
        float width = paint.measureText(text) + dp(32);
        float right = getWidth() - dp(5);
        float top = dp(5);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(0xee07152e);
        canvas.drawRoundRect(right - width, top, right, top + dp(20), dp(10), dp(10), paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(dp(1));
        paint.setColor(color);
        canvas.drawRoundRect(right - width, top, right, top + dp(20), dp(10), dp(10), paint);
        float cx = right - width + dp(11), cy = top + dp(10);
        canvas.drawCircle(cx, cy + dp(4), dp(1), paint);
        canvas.drawArc(cx - dp(4), cy, cx + dp(4), cy + dp(8), 205, 130, false, paint);
        canvas.drawArc(cx - dp(7), cy - dp(3), cx + dp(7), cy + dp(11), 205, 130, false, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(color);
        canvas.drawText(text, right - width + dp(23), top + dp(14), paint);
    }

    private RectF fit(float iw, float ih, float w, float h) {
        float scale = Math.min(w / iw, h / ih);
        float dw = iw * scale, dh = ih * scale;
        return new RectF((w - dw) / 2, (h - dh) / 2, (w + dw) / 2, (h + dh) / 2);
    }

    private float x(RectF dst, float fraction) { return dst.left + dst.width() * fraction; }
    private float y(RectF dst, float fraction) { return dst.top + dst.height() * fraction; }

    private float dp(float value) {
        float density = getResources().getDisplayMetrics().density;
        float widthDp = getWidth() > 0
                ? getWidth() / density
                : getResources().getDisplayMetrics().widthPixels / density;
        float scale = Math.max(1f, Math.min(1.35f, widthDp / 400f));
        return value * density * scale;
    }
}
