package com.lordofrobots.telemetry;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.Locale;
import java.util.UUID;

public final class MainActivity extends Activity {
    private static final UUID SERVICE_UUID =
            UUID.fromString("8b7d0001-3f9b-4f6f-8d6a-11f6a3c80001");
    private static final UUID ROBOT_UUID =
            UUID.fromString("8b7d0002-3f9b-4f6f-8d6a-11f6a3c80001");
    private static final UUID CONTROLLER_UUID =
            UUID.fromString("8b7d0003-3f9b-4f6f-8d6a-11f6a3c80001");
    private static final UUID CCCD_UUID =
            UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private static final int PERMISSION_REQUEST = 41;

    private static final int BG = Color.rgb(4, 12, 28);
    private static final int CARD = Color.rgb(8, 24, 54);
    private static final int CARD_BORDER = Color.rgb(18, 61, 128);
    private static final int TEXT = Color.rgb(245, 249, 255);
    private static final int MUTED = Color.rgb(130, 150, 182);
    private static final int GREEN = Color.rgb(69, 212, 131);
    private static final int RED = Color.rgb(255, 89, 100);
    private static final int CYAN = Color.rgb(84, 220, 255);
    private static final int AMBER = Color.rgb(240, 180, 41);
    private static final int BRAND = Color.rgb(11, 59, 145);

    private final Handler handler = new Handler(Looper.getMainLooper());
    private BluetoothAdapter adapter;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic robotCharacteristic;
    private BluetoothGattCharacteristic controllerCharacteristic;
    private boolean scanning;
    private long lastPacketAt;
    private int packetCount;
    private int droppedPackets;
    private int previousSequence = -1;

    private TextView connectionView;
    private TextView packetView;
    private LinearLayout statusCard;
    private TextView statusView;
    private TextView batteryValue;
    private TextView batteryDetail;
    private TextView rawController;
    private BatteryGraphView batteryGraph;
    private RobotTelemetryView robotView;
    private GamepadTelemetryView gamepadView;
    private SafetyTelemetryView safetyView;
    private Button connectButton;
    private float interfaceScale = 1f;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        float widthDp = getResources().getDisplayMetrics().widthPixels /
                getResources().getDisplayMetrics().density;
        interfaceScale = Math.max(1f, Math.min(1.25f, widthDp / 420f));
        getWindow().setStatusBarColor(BG);
        getWindow().setNavigationBarColor(BG);
        buildInterface();
        BluetoothManager manager = getSystemService(BluetoothManager.class);
        adapter = manager == null ? null : manager.getAdapter();
        handler.post(staleMonitor);
        if (hasBluetoothPermissions()) startScan(); else requestBluetoothPermissions();
    }

    private void buildInterface() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setPadding(dp(10), dp(8), dp(10), dp(8));
        root.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override public WindowInsets onApplyWindowInsets(View view, WindowInsets insets) {
                android.graphics.Insets bars = insets.getInsets(WindowInsets.Type.systemBars());
                view.setPadding(dp(10), bars.top + dp(6), dp(10), bars.bottom + dp(6));
                return insets;
            }
        });

        // Use the supplied transparent cobalt wordmark at its native aspect ratio.
        LinearLayout brandHeader = new LinearLayout(this);
        brandHeader.setGravity(Gravity.CENTER_VERTICAL);
        brandHeader.setPadding(dp(8), 0, dp(8), 0);
        brandHeader.setBackground(rounded(Color.WHITE, dp(10), Color.TRANSPARENT));
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.lor_logo_cobalt);
        logo.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        brandHeader.addView(logo, new LinearLayout.LayoutParams(dp(190), dp(40)));
        TextView eyebrow = label("COMBAT SYSTEM\nTELEMETRY", 10, BRAND, true);
        eyebrow.setGravity(Gravity.CENTER_VERTICAL | Gravity.END);
        eyebrow.setLetterSpacing(.06f);
        brandHeader.addView(eyebrow, new LinearLayout.LayoutParams(0, dp(40), 1));
        LinearLayout.LayoutParams brandParams = new LinearLayout.LayoutParams(-1, dp(44));
        brandParams.setMargins(0, 0, 0, dp(6));
        root.addView(brandHeader, brandParams);

        LinearLayout connectionCard = card();
        connectionCard.setOrientation(LinearLayout.HORIZONTAL);
        connectionCard.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout connectionText = new LinearLayout(this);
        connectionText.setOrientation(LinearLayout.VERTICAL);
        connectionView = label("SEARCHING FOR ROBOT", 14, AMBER, true);
        packetView = label("No telemetry received", 9, MUTED, false);
        connectionText.addView(connectionView);
        connectionText.addView(packetView);
        connectButton = new Button(this);
        connectButton.setText("SCAN");
        connectButton.setTextColor(Color.WHITE);
        connectButton.setTextSize(11 * interfaceScale);
        connectButton.setTypeface(Typeface.DEFAULT_BOLD);
        connectButton.setBackground(rounded(BRAND, dp(10), Color.TRANSPARENT));
        connectButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View view) { startScan(); }
        });
        connectionCard.addView(connectionText, new LinearLayout.LayoutParams(0, -2, 1));
        connectionCard.addView(connectButton, new LinearLayout.LayoutParams(dp(86), dp(38)));
        fixedHeight(connectionCard, 56);
        root.addView(connectionCard);

        statusCard = new LinearLayout(this);
        statusCard.setGravity(Gravity.CENTER);
        statusCard.setPadding(dp(12), 0, dp(12), 0);
        statusView = label("SEARCHING FOR ROBOT", 15, Color.rgb(33, 24, 0), true);
        statusView.setGravity(Gravity.CENTER);
        statusView.setLetterSpacing(.04f);
        statusCard.addView(statusView, new LinearLayout.LayoutParams(-1, -2));
        LinearLayout.LayoutParams statusParams = new LinearLayout.LayoutParams(-1, dp(44));
        statusParams.setMargins(0, 0, 0, dp(6));
        statusCard.setLayoutParams(statusParams);
        root.addView(statusCard);
        setRobotStatus("SEARCHING FOR ROBOT", AMBER);

        LinearLayout batteryCard = card();
        batteryCard.addView(sectionTitle("MAIN BATTERY  /  3S HV LIPO  /  30 SEC"));
        LinearLayout batteryHeader = new LinearLayout(this);
        batteryHeader.setGravity(Gravity.BOTTOM);
        batteryValue = label("--.-- V", 25, MUTED, true);
        batteryDetail = label("GPIO8  •  waiting", 9, MUTED, true);
        batteryDetail.setGravity(Gravity.END);
        batteryHeader.addView(batteryValue, new LinearLayout.LayoutParams(0, -2, 1));
        batteryHeader.addView(batteryDetail, new LinearLayout.LayoutParams(0, -2, 1));
        batteryCard.addView(batteryHeader);
        batteryGraph = new BatteryGraphView(this);
        batteryCard.addView(batteryGraph, new LinearLayout.LayoutParams(-1, 0, 1));
        weightedHeight(batteryCard, 0.85f);
        root.addView(batteryCard);

        LinearLayout robotCard = card();
        robotCard.addView(sectionTitle("ROBOT LIVE VIEW"));
        robotView = new RobotTelemetryView(this);
        robotCard.addView(robotView, new LinearLayout.LayoutParams(-1, 0, 1));
        weightedHeight(robotCard, 1.30f);
        root.addView(robotCard);

        LinearLayout systemCard = card();
        systemCard.addView(sectionTitle("SAFETY + OUTPUT STATE"));
        safetyView = new SafetyTelemetryView(this);
        systemCard.addView(safetyView, new LinearLayout.LayoutParams(-1, 0, 1));
        fixedHeight(systemCard, 96);
        root.addView(systemCard);

        LinearLayout gamepadCard = card();
        gamepadCard.addView(sectionTitle("GAMEPAD LIVE VIEW  /  ALL INPUTS"));
        gamepadView = new GamepadTelemetryView(this);
        gamepadCard.addView(gamepadView, new LinearLayout.LayoutParams(-1, 0, 1));
        rawController = label("", 1, MUTED, false);  // Raw values are drawn on the gamepad view.
        weightedHeight(gamepadCard, 1.65f);
        root.addView(gamepadCard);

        setContentView(root);
        showDisconnectedValues();
    }

    private LinearLayout card() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(11), dp(8), dp(11), dp(8));
        card.setBackground(rounded(CARD, dp(18), CARD_BORDER));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
        params.setMargins(0, 0, 0, dp(6));
        card.setLayoutParams(params);
        return card;
    }

    private TextView sectionTitle(String text) {
        TextView view = label(text, 12, CYAN, true);
        view.setLetterSpacing(.08f);
        view.setPadding(0, 0, 0, dp(3));
        return view;
    }

    private void fixedHeight(LinearLayout card, int heightDp) {
        LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) card.getLayoutParams();
        params.height = dp(heightDp);
        card.setLayoutParams(params);
    }

    private void weightedHeight(LinearLayout card, float weight) {
        LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) card.getLayoutParams();
        params.height = 0;
        params.weight = weight;
        card.setLayoutParams(params);
    }

    private TextView label(String value, int sp, int color, boolean bold) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(sp * interfaceScale);
        view.setTextColor(color);
        if (bold) view.setTypeface(Typeface.DEFAULT_BOLD);
        return view;
    }

    private GradientDrawable rounded(int fill, int radius, int stroke) {
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(fill);
        shape.setCornerRadius(radius);
        if (stroke != Color.TRANSPARENT) shape.setStroke(dp(1), stroke);
        return shape;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density * interfaceScale);
    }

    private boolean hasBluetoothPermissions() {
        if (Build.VERSION.SDK_INT < 31) {
            return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                    == PackageManager.PERMISSION_GRANTED;
        }
        return checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
                && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestBluetoothPermissions() {
        if (Build.VERSION.SDK_INT < 31) {
            requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, PERMISSION_REQUEST);
        } else {
            requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT}, PERMISSION_REQUEST);
        }
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == PERMISSION_REQUEST && hasBluetoothPermissions()) startScan();
        else setConnection("BLUETOOTH PERMISSION REQUIRED", RED);
    }

    private void startScan() {
        if (!hasBluetoothPermissions()) { requestBluetoothPermissions(); return; }
        if (adapter == null || !adapter.isEnabled()) { setConnection("TURN ON BLUETOOTH", RED); return; }
        closeGatt();
        scanner = adapter.getBluetoothLeScanner();
        if (scanner == null) { setConnection("BLE SCANNER UNAVAILABLE", RED); return; }
        scanning = true;
        setConnection("SEARCHING FOR LOR SSS", AMBER);
        packetView.setText("Looking for the robot telemetry service…");
        ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
        scanner.startScan(null, settings, scanCallback);
        handler.postDelayed(new Runnable() {
            @Override public void run() {
                if (scanning) {
                    stopScan();
                    setConnection("ROBOT NOT FOUND", RED);
                    packetView.setText("Check robot power, then scan again");
                }
            }
        }, 12000);
    }

    private void stopScan() {
        if (scanning && scanner != null && hasBluetoothPermissions()) scanner.stopScan(scanCallback);
        scanning = false;
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override public void onScanResult(int callbackType, ScanResult result) {
            if (!scanning) return;
            String name = result.getScanRecord() == null ? null : result.getScanRecord().getDeviceName();
            boolean nameMatches = name != null && name.startsWith("LoR SSS");
            boolean serviceMatches = result.getScanRecord() != null
                    && result.getScanRecord().getServiceUuids() != null
                    && result.getScanRecord().getServiceUuids().contains(new ParcelUuid(SERVICE_UUID));
            if (!nameMatches && !serviceMatches) return;
            stopScan();
            setConnection("CONNECTING", AMBER);
            packetView.setText(result.getDevice().getAddress());
            gatt = result.getDevice().connectGatt(MainActivity.this, false, gattCallback,
                    BluetoothDevice.TRANSPORT_LE);
        }

        @Override public void onScanFailed(int errorCode) {
            scanning = false;
            setConnection("SCAN FAILED " + errorCode, RED);
        }
    };

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt callbackGatt, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
                runOnUiThread(new Runnable() {
                    @Override public void run() { setConnection("DISCOVERING TELEMETRY", AMBER); }
                });
                callbackGatt.discoverServices();
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                runOnUiThread(new Runnable() {
                    @Override public void run() {
                        setConnection("ROBOT DISCONNECTED", RED);
                        packetView.setText("Tap scan to reconnect");
                        showDisconnectedValues();
                    }
                });
            }
        }

        @Override public void onServicesDiscovered(BluetoothGatt callbackGatt, int status) {
            BluetoothGattService service = status == BluetoothGatt.GATT_SUCCESS
                    ? callbackGatt.getService(SERVICE_UUID) : null;
            robotCharacteristic = service == null ? null : service.getCharacteristic(ROBOT_UUID);
            controllerCharacteristic = service == null ? null : service.getCharacteristic(CONTROLLER_UUID);
            if (robotCharacteristic == null || controllerCharacteristic == null) {
                runOnUiThread(new Runnable() {
                    @Override public void run() { setConnection("TELEMETRY SERVICE MISSING", RED); }
                });
                return;
            }
            subscribe(callbackGatt, robotCharacteristic);
        }

        @Override public void onDescriptorWrite(BluetoothGatt callbackGatt,
                                                  BluetoothGattDescriptor descriptor, int status) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                runOnUiThread(new Runnable() {
                    @Override public void run() { setConnection("SUBSCRIPTION FAILED", RED); }
                });
                return;
            }
            if (descriptor.getCharacteristic().getUuid().equals(ROBOT_UUID)) {
                subscribe(callbackGatt, controllerCharacteristic);
            } else {
                runOnUiThread(new Runnable() {
                    @Override public void run() {
                        setConnection("ROBOT CONNECTED", GREEN);
                        packetView.setText("Robot + controller streams active");
                    }
                });
            }
        }

        @Override public void onCharacteristicChanged(BluetoothGatt callbackGatt,
                                                        BluetoothGattCharacteristic characteristic,
                                                        byte[] value) {
            processTelemetry(characteristic.getUuid(), value);
        }

        @Override @SuppressWarnings("deprecation")
        public void onCharacteristicChanged(BluetoothGatt callbackGatt,
                                             BluetoothGattCharacteristic characteristic) {
            processTelemetry(characteristic.getUuid(), characteristic.getValue());
        }
    };

    @SuppressWarnings("deprecation")
    private void subscribe(BluetoothGatt callbackGatt, BluetoothGattCharacteristic characteristic) {
        BluetoothGattDescriptor cccd = characteristic.getDescriptor(CCCD_UUID);
        if (cccd == null) { setConnection("CCCD MISSING", RED); return; }
        callbackGatt.setCharacteristicNotification(characteristic, true);
        if (Build.VERSION.SDK_INT >= 33) {
            callbackGatt.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
        } else {
            cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            callbackGatt.writeDescriptor(cccd);
        }
    }

    private void processTelemetry(UUID uuid, byte[] value) {
        if (ROBOT_UUID.equals(uuid)) {
            TelemetryPacket packet = TelemetryPacket.parse(value);
            if (packet == null || packet.version != 2) return;
            runOnUiThread(new Runnable() {
                @Override public void run() { showRobotPacket(packet); }
            });
        } else if (CONTROLLER_UUID.equals(uuid)) {
            ControllerTelemetryPacket packet = ControllerTelemetryPacket.parse(value);
            if (packet == null || packet.version != 1) return;
            runOnUiThread(new Runnable() {
                @Override public void run() { showControllerPacket(packet); }
            });
        }
    }

    private void showRobotPacket(TelemetryPacket p) {
        long now = System.currentTimeMillis();
        lastPacketAt = now;
        packetCount++;
        if (previousSequence >= 0 && p.sequence != previousSequence) {
            int expected = (previousSequence + 1) & 0xff;
            droppedPackets += (p.sequence - expected) & 0xff;
        }
        previousSequence = p.sequence;
        setConnection("ROBOT CONNECTED", GREEN);
        String inputAge = p.controllerAgeMs >= 65535
                ? "N/A"
                : String.format(Locale.US, "%d ms", p.controllerAgeMs);
        packetView.setText(String.format(Locale.US,
                "Packets %,d  •  skipped %,d  •  input age %s",
                packetCount, droppedPackets, inputAge));

        float volts = p.batteryMillivolts / 1000f;
        int batteryColor = volts < 1f ? MUTED : BatteryGraphView.conditionColor(p.batteryPercent);
        batteryValue.setText(volts < 1f ? "NO PACK" : String.format(Locale.US, "%.2f V", volts));
        batteryValue.setTextColor(batteryColor);
        batteryDetail.setText(String.format(Locale.US, "%d%%  •  ADC %.3f V",
                p.batteryPercent, p.adcMillivolts / 1000f));
        batteryDetail.setTextColor(batteryColor);
        if (volts >= 1f) batteryGraph.addSample(volts, now);

        if (p.flag(16)) {
            setRobotStatus("ERROR  •  SYSTEM FAULT", RED);
        } else if (volts >= 1f && p.batteryPercent <= 25) {
            setRobotStatus("ERROR  •  BATTERY CRITICAL", RED);
        } else if (p.flag(2)) {
            setRobotStatus("SAFE  •  MOTION FAIL-SAFE LATCHED", AMBER);
        } else if (!p.flag(1)) {
            setRobotStatus("WARNING  •  GAMEPAD DISCONNECTED", AMBER);
        } else if (volts < 1f) {
            setRobotStatus("WARNING  •  BATTERY MONITOR OFFLINE", AMBER);
        } else if (p.batteryPercent <= 50) {
            setRobotStatus("WARNING  •  BATTERY LOW", AMBER);
        } else if (p.flag(64)) {
            setRobotStatus("WARNING  •  WEAPON ESC ARMING", AMBER);
        } else if (p.flag(8) && p.weaponCommand != 0) {
            setRobotStatus("NOMINAL  •  WEAPON RUNNING", GREEN);
        } else {
            setRobotStatus("NOMINAL  •  ROBOT READY", GREEN);
        }

        safetyView.setPacket(p);
        gamepadView.setConnected(p.flag(1));
        robotView.setPacket(p);
    }

    private void showControllerPacket(ControllerTelemetryPacket p) {
        gamepadView.setPacket(p);
        rawController.setText(String.format(Locale.US,
                "LX %+4d  LY %+4d   RX %+4d  RY %+4d\nLT %4d  RT %4d   DPAD 0x%X\nBUTTONS 0x%04X   MISC 0x%04X",
                p.axisX, p.axisY, p.axisRX, p.axisRY,
                p.brake, p.throttle, p.dpad, p.buttons, p.miscButtons));
    }

    private void setConnection(String text, int color) {
        connectionView.setText(text);
        connectionView.setTextColor(color);
        if (color == RED) {
            setRobotStatus("ERROR  •  " + text, RED);
        } else if (color == AMBER) {
            setRobotStatus(text, AMBER);
        }
    }

    private void setRobotStatus(String text, int color) {
        if (statusCard == null || statusView == null) return;
        statusCard.setBackground(rounded(color, dp(14), Color.TRANSPARENT));
        statusView.setText(text);
        statusView.setTextColor(color == RED ? Color.WHITE : Color.rgb(4, 18, 20));
    }

    private void showDisconnectedValues() {
        safetyView.setPacket(null);
        batteryValue.setText("--.-- V");
        batteryValue.setTextColor(MUTED);
        batteryDetail.setText("GPIO8  •  waiting");
        rawController.setText("Axes and buttons waiting for data");
        robotView.setPacket(null);
        gamepadView.setPacket(null);
        gamepadView.setConnected(false);
    }

    private final Runnable staleMonitor = new Runnable() {
        @Override public void run() {
            if (lastPacketAt != 0 && System.currentTimeMillis() - lastPacketAt > 1600) {
                setConnection("TELEMETRY STALE", RED);
            }
            handler.postDelayed(this, 500);
        }
    };

    private void closeGatt() {
        if (gatt != null) {
            gatt.disconnect();
            gatt.close();
            gatt = null;
        }
        robotCharacteristic = null;
        controllerCharacteristic = null;
        packetCount = 0;
        droppedPackets = 0;
        previousSequence = -1;
        lastPacketAt = 0;
        batteryGraph.clearSamples();
    }

    @Override protected void onDestroy() {
        stopScan();
        closeGatt();
        handler.removeCallbacksAndMessages(null);
        super.onDestroy();
    }
}
