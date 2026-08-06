package com.lordofrobots.telemetry;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

final class TelemetryPacket {
    static final int SIZE = 20;

    final int version;
    final int flags;
    final int sequence;
    final int loopHz;
    final int batteryMillivolts;
    final int adcMillivolts;
    final int driveLeft;
    final int driveRight;
    final int weaponCommand;
    final int weaponDshot;
    final int controllerAgeMs;
    final int batteryPercent;

    private TelemetryPacket(ByteBuffer b) {
        version = b.get() & 0xff;
        flags = b.get() & 0xff;
        sequence = b.get() & 0xff;
        loopHz = b.get() & 0xff;
        batteryMillivolts = b.getShort() & 0xffff;
        adcMillivolts = b.getShort() & 0xffff;
        driveLeft = b.getShort();
        driveRight = b.getShort();
        weaponCommand = b.getShort();
        weaponDshot = b.getShort() & 0xffff;
        controllerAgeMs = b.getShort() & 0xffff;
        batteryPercent = b.get() & 0xff;
        b.get();
    }

    static TelemetryPacket parse(byte[] value) {
        if (value == null || value.length != SIZE) return null;
        return new TelemetryPacket(ByteBuffer.wrap(value).order(ByteOrder.LITTLE_ENDIAN));
    }

    boolean flag(int mask) {
        return (flags & mask) != 0;
    }
}
