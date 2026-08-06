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

final class RobotTelemetryView extends View {
    private final Bitmap stationaryCore;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private TelemetryPacket packet;
    private float shellAngle;
    private long lastFrameNanos;

    RobotTelemetryView(Context context) {
        super(context);
        stationaryCore = BitmapFactory.decodeResource(
                getResources(), R.drawable.robot_core_stationary);
    }

    void setPacket(TelemetryPacket packet) {
        this.packet = packet;
        lastFrameNanos = 0;
        invalidate();
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float w = getWidth(), h = getHeight();
        RectF dst = fit(stationaryCore.getWidth(), stationaryCore.getHeight(), w, h);
        canvas.drawBitmap(stationaryCore, null, dst, paint);

        updateShellAngle();
        drawRotatingShell(canvas, dst);

        if (packet == null) {
            badge(canvas, w * .5f, h * .91f, "WAITING FOR ROBOT", 0xfff0b429);
            return;
        }

        driveMeter(canvas, w * .04f, w * .47f, h * .86f, packet.driveLeft, "LEFT TRACK");
        driveMeter(canvas, w * .53f, w * .96f, h * .86f, packet.driveRight, "RIGHT TRACK");
        String direction = packet.weaponCommand < 0 ? "REVERSE" :
                (packet.weaponCommand > 0 ? "FORWARD" : "STOPPED");
        int weaponColor = packet.weaponCommand == 0 ? 0xff45d483 :
                (packet.weaponCommand < 0 ? 0xffff5964 : 0xff53dcff);
        badge(canvas, w * .5f, h * .46f,
                String.format(Locale.US, "%s  %d", direction, packet.weaponDshot), weaponColor);
        badge(canvas, w * .76f, h * .13f,
                String.format(Locale.US, "%.2f V  %d%%",
                        packet.batteryMillivolts / 1000f, packet.batteryPercent),
                packet.batteryMillivolts < 1000
                        ? 0xff8296b6 : BatteryGraphView.conditionColor(packet.batteryPercent));

        int escColor = packet.flag(64) ? 0xffffd34e :
                (packet.flag(8) ? 0xff45d483 : 0xffff5964);
        badge(canvas, w * .24f, h * .13f,
                packet.flag(64) ? "ESC ARMING" :
                        (packet.flag(8) ? "ESC ENABLED" : "ESC DISABLED"), escColor);
    }

    private void updateShellAngle() {
        long now = System.nanoTime();
        if (lastFrameNanos == 0) lastFrameNanos = now;
        float dt = Math.min(.05f, (now - lastFrameNanos) / 1_000_000_000f);
        lastFrameNanos = now;
        int command = packet == null ? 0 : packet.weaponCommand;
        float speed = Math.min(1f, Math.abs(command) / 90f);
        if (speed > .01f) {
            float degreesPerSecond = 90f + speed * 990f;
            shellAngle = (shellAngle + Math.signum(command) * degreesPerSecond * dt) % 360f;
            postInvalidateOnAnimation();
        }
    }

    private void drawRotatingShell(Canvas canvas, RectF dst) {
        float size = Math.min(dst.width(), dst.height());
        float cx = dst.left + dst.width() * .50f;
        float cy = dst.top + dst.height() * .46f;
        float radius = size * .355f;
        float inner = size * .055f;
        int saved = canvas.save();
        canvas.rotate(shellAngle, cx, cy);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setColor(0xff5d5954);
        paint.setStrokeWidth(size * .105f);
        for (int arm = 0; arm < 3; arm++) {
            canvas.drawLine(cx + inner, cy, cx + radius - size * .025f, cy, paint);
            canvas.rotate(120f, cx, cy);
        }

        paint.setStrokeWidth(size * .045f);
        canvas.drawCircle(cx, cy, radius, paint);
        paint.setColor(0xff8b8780);
        paint.setStrokeWidth(size * .008f);
        canvas.drawCircle(cx, cy, radius - size * .021f, paint);

        // Dark moving vent marks help make rotation readable at low speed.
        paint.setColor(0xff191c22);
        paint.setStrokeWidth(size * .015f);
        for (int arm = 0; arm < 3; arm++) {
            float x1 = cx + radius * .40f;
            float x2 = cx + radius * .66f;
            canvas.drawLine(x1, cy - size * .018f, x2, cy - size * .018f, paint);
            canvas.drawLine(x1, cy + size * .018f, x2, cy + size * .018f, paint);
            canvas.rotate(120f, cx, cy);
        }
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(0xff66615c);
        canvas.drawCircle(cx, cy, size * .065f, paint);
        paint.setColor(0xffb7b9bb);
        canvas.drawCircle(cx, cy, size * .020f, paint);
        canvas.restoreToCount(saved);
    }

    private void driveMeter(Canvas c, float left, float right, float y, int value, String name) {
        float center = (left + right) / 2f;
        float barTop = y;
        float barBottom = y + dp(8);
        paint.setColor(0xdd07152e);
        c.drawRoundRect(left, barTop, right, barBottom, dp(4), dp(4), paint);

        float normalized = Math.max(-1f, Math.min(1f, value / 512f));
        float end = center + normalized * (right - left) / 2f;
        paint.setColor(value < 0 ? 0xffffd34e : (value > 0 ? 0xff54dcff : 0xff8296b6));
        c.drawRoundRect(Math.min(center, end), barTop, Math.max(center, end), barBottom,
                dp(4), dp(4), paint);
        paint.setColor(0xffd5e3f7);
        c.drawRect(center - dp(.5f), barTop - dp(1), center + dp(.5f), barBottom + dp(1), paint);

        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        paint.setTextSize(dp(8));
        paint.setColor(Color.WHITE);
        c.drawText(String.format(Locale.US, "%s  %+d", name, value), center, y - dp(4), paint);
    }

    private void badge(Canvas c, float x, float y, String text, int color) {
        paint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        paint.setTextSize(dp(10));
        paint.setTextAlign(Paint.Align.CENTER);
        float width = paint.measureText(text) + dp(18);
        paint.setColor(0xe607152e);
        c.drawRoundRect(x - width / 2, y - dp(14), x + width / 2, y + dp(7),
                dp(10), dp(10), paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(dp(1));
        paint.setColor(color);
        c.drawRoundRect(x - width / 2, y - dp(14), x + width / 2, y + dp(7),
                dp(10), dp(10), paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(color);
        c.drawText(text, x, y + dp(1), paint);
    }

    private RectF fit(float iw, float ih, float w, float h) {
        float scale = Math.min(w / iw, h / ih);
        float dw = iw * scale, dh = ih * scale;
        return new RectF((w - dw) / 2, (h - dh) / 2, (w + dw) / 2, (h + dh) / 2);
    }

    private float dp(float value) {
        float density = getResources().getDisplayMetrics().density;
        float widthDp = getWidth() > 0
                ? getWidth() / density
                : getResources().getDisplayMetrics().widthPixels / density;
        float scale = Math.max(1f, Math.min(1.35f, widthDp / 400f));
        return value * density * scale;
    }
}
