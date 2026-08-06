package com.lordofrobots.telemetry;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.View;

final class SafetyTelemetryView extends View {
    private static final int GREEN = 0xff45d483;
    private static final int RED = 0xffff5964;
    private static final int AMBER = 0xfff0b429;
    private static final int MUTED = 0xff8296b6;
    private static final int TILE = 0xff07152e;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path path = new Path();
    private TelemetryPacket packet;

    SafetyTelemetryView(Context context) { super(context); }

    void setPacket(TelemetryPacket packet) {
        this.packet = packet;
        invalidate();
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float gap = dp(3);
        float tileW = (getWidth() - gap * 2) / 3f;
        float tileH = (getHeight() - gap) / 2f;

        boolean connected = packet != null && packet.flag(1);
        boolean failsafeClear = packet != null && !packet.flag(2);
        boolean driveEnabled = packet != null && packet.flag(4);
        boolean weaponEnabled = packet != null && packet.flag(8);
        boolean weaponArming = packet != null && packet.flag(64);
        boolean faultClear = packet != null && !packet.flag(16);
        int loopHz = packet == null ? 0 : packet.loopHz;

        tile(canvas, 0, 0, tileW, tileH, "GAMEPAD",
                packet == null ? "NO DATA" : (connected ? "LINKED" : "WAITING"),
                packet == null ? MUTED : (connected ? GREEN : AMBER), 0);
        tile(canvas, tileW + gap, 0, tileW, tileH, "FAIL-SAFE",
                packet == null ? "—" : (failsafeClear ? "CLEAR" : "LATCHED"),
                packet == null ? MUTED : (failsafeClear ? GREEN : RED), 1);
        tile(canvas, (tileW + gap) * 2, 0, tileW, tileH, "FAULT",
                packet == null ? "—" : (faultClear ? "NONE" : "FAULT"),
                packet == null ? MUTED : (faultClear ? GREEN : RED), 2);
        tile(canvas, 0, tileH + gap, tileW, tileH, "DRIVE",
                packet == null ? "—" : (driveEnabled ? "ENABLED" : "DISABLED"),
                packet == null ? MUTED : (driveEnabled ? GREEN : RED), 3);
        tile(canvas, tileW + gap, tileH + gap, tileW, tileH, "WEAPON ESC",
                packet == null ? "—" : (weaponArming ? "ARMING" :
                        (weaponEnabled ? "ENABLED" : "DISABLED")),
                packet == null ? MUTED : (weaponArming ? AMBER :
                        (weaponEnabled ? GREEN : RED)), 4);
        int loopColor = packet == null ? MUTED : (loopHz >= 90 ? GREEN : (loopHz >= 70 ? AMBER : RED));
        tile(canvas, (tileW + gap) * 2, tileH + gap, tileW, tileH, "CONTROL LOOP",
                packet == null ? "—" : loopHz + " Hz", loopColor, 5);
    }

    private void tile(Canvas canvas, float left, float top, float width, float height,
                      String title, String value, int color, int icon) {
        float inset = dp(1);
        RectF box = new RectF(left + inset, top + inset,
                left + width - inset, top + height - inset);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(TILE);
        canvas.drawRoundRect(box, dp(7), dp(7), paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(dp(1));
        paint.setColor(withAlpha(color, 135));
        canvas.drawRoundRect(box, dp(7), dp(7), paint);

        float iconX = box.left + dp(12);
        float iconY = box.centerY();
        drawIcon(canvas, icon, iconX, iconY, color);

        float textX = box.left + dp(24);
        paint.setStyle(Paint.Style.FILL);
        paint.setTextAlign(Paint.Align.LEFT);
        paint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        paint.setTextSize(dp(6.5f));
        paint.setColor(MUTED);
        canvas.drawText(title, textX, iconY - dp(2), paint);
        paint.setTextSize(dp(8.5f));
        paint.setColor(color);
        canvas.drawText(value, textX, iconY + dp(8), paint);
    }

    private void drawIcon(Canvas canvas, int icon, float x, float y, int color) {
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(dp(1.5f));
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setColor(color);
        float r = dp(7);
        switch (icon) {
            case 0: // gamepad
                canvas.drawRoundRect(x - r, y - dp(4), x + r, y + dp(5), dp(4), dp(4), paint);
                canvas.drawLine(x - dp(4), y, x, y, paint);
                canvas.drawLine(x - dp(2), y - dp(2), x - dp(2), y + dp(2), paint);
                canvas.drawCircle(x + dp(4), y - dp(1), dp(.8f), paint);
                break;
            case 1: // shield
                path.reset();
                path.moveTo(x, y - r); path.lineTo(x + dp(6), y - dp(4));
                path.lineTo(x + dp(5), y + dp(3)); path.lineTo(x, y + r);
                path.lineTo(x - dp(5), y + dp(3)); path.lineTo(x - dp(6), y - dp(4));
                path.close(); canvas.drawPath(path, paint);
                canvas.drawLine(x - dp(3), y, x - dp(1), y + dp(2), paint);
                canvas.drawLine(x - dp(1), y + dp(2), x + dp(4), y - dp(3), paint);
                break;
            case 2: // warning triangle
                path.reset(); path.moveTo(x, y - r); path.lineTo(x + r, y + dp(6));
                path.lineTo(x - r, y + dp(6)); path.close(); canvas.drawPath(path, paint);
                canvas.drawLine(x, y - dp(3), x, y + dp(1), paint);
                canvas.drawPoint(x, y + dp(4), paint);
                break;
            case 3: // tracks / motion
                canvas.drawRoundRect(x - r, y - dp(5), x + r, y + dp(5), dp(5), dp(5), paint);
                canvas.drawLine(x - dp(4), y, x + dp(4), y, paint);
                canvas.drawLine(x + dp(1), y - dp(3), x + dp(4), y, paint);
                canvas.drawLine(x + dp(1), y + dp(3), x + dp(4), y, paint);
                break;
            case 4: // spinner
                canvas.drawCircle(x, y, r, paint);
                for (int i = 0; i < 3; i++) {
                    double a = i * Math.PI * 2 / 3;
                    canvas.drawLine(x, y, x + (float)Math.cos(a) * r,
                            y + (float)Math.sin(a) * r, paint);
                }
                break;
            default: // loop pulse
                path.reset(); path.moveTo(x - r, y); path.lineTo(x - dp(4), y);
                path.lineTo(x - dp(2), y - dp(4)); path.lineTo(x + dp(1), y + dp(5));
                path.lineTo(x + dp(3), y); path.lineTo(x + r, y); canvas.drawPath(path, paint);
                break;
        }
        paint.setStrokeCap(Paint.Cap.BUTT);
    }

    private int withAlpha(int color, int alpha) {
        return Color.argb(alpha, Color.red(color), Color.green(color), Color.blue(color));
    }

    private float dp(float value) {
        float density = getResources().getDisplayMetrics().density;
        float widthDp = getWidth() > 0 ? getWidth() / density
                : getResources().getDisplayMetrics().widthPixels / density;
        float scale = Math.max(1f, Math.min(1.35f, widthDp / 400f));
        return value * density * scale;
    }
}
