package com.lordofrobots.telemetry;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

final class ControllerTelemetryPacket {
    static final int SIZE = 20;

    final int version;
    final int sequence;
    final int dpad;
    final int battery;
    final int axisX;
    final int axisY;
    final int axisRX;
    final int axisRY;
    final int throttle;
    final int brake;
    final int buttons;
    final int miscButtons;

    private ControllerTelemetryPacket(ByteBuffer b) {
        version = b.get() & 0xff;
        sequence = b.get() & 0xff;
        dpad = b.get() & 0xff;
        battery = b.get() & 0xff;
        axisX = b.getShort();
        axisY = b.getShort();
        axisRX = b.getShort();
        axisRY = b.getShort();
        throttle = b.getShort() & 0xffff;
        brake = b.getShort() & 0xffff;
        buttons = b.getShort() & 0xffff;
        miscButtons = b.getShort() & 0xffff;
    }

    static ControllerTelemetryPacket parse(byte[] value) {
        if (value == null || value.length != SIZE) return null;
        return new ControllerTelemetryPacket(
                ByteBuffer.wrap(value).order(ByteOrder.LITTLE_ENDIAN));
    }

    boolean button(int mask) { return (buttons & mask) != 0; }
    boolean misc(int mask) { return (miscButtons & mask) != 0; }
}
