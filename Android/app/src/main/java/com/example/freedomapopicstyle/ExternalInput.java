package com.example.freedomapopicstyle;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/** キーボード型の太鼓・ゲームパッド。割り当ては UI スレッド、打鍵はゲームスレッドへ渡す。 */
final class ExternalInput {
    private final Activity activity;
    private final SharedPreferences prefs;
    private final AtomicInteger hits = new AtomicInteger();
    private final Set<Integer> held = new HashSet<>();
    private final Set<String>[] bindings;
    private int capture = -1;
    private AlertDialog captureDialog;
    private static final String[] LANES = { "左カッ", "左ドン", "右ドン", "右カッ" };
    private static final int[] AXES = { MotionEvent.AXIS_X, MotionEvent.AXIS_Y,
        MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER };

    @SuppressWarnings("unchecked")
    ExternalInput(Activity activity) {
        this.activity = activity;
        prefs = activity.getSharedPreferences("external-input", Activity.MODE_PRIVATE);
        bindings = new Set[4];
        for (int i = 0; i < 4; i++) { bindings[i] = new HashSet<>(prefs.getStringSet("lane" + i, new HashSet<>())); }
    }

    void configure() {
        String[] rows = new String[4];
        for (int i = 0; i < 4; i++) { rows[i] = LANES[i] + " (" + bindings[i].size() + " 個)"; }
        new AlertDialog.Builder(activity).setTitle("外部入力のキー設定")
            .setItems(rows, (dialog, lane) -> new AlertDialog.Builder(activity).setTitle(LANES[lane])
                .setItems(new String[] { "キー・ボタン・軸を追加", "このレーンの割り当てを消去" }, (d, action) -> {
                    if (action == 1) { bindings[lane].clear(); save(); return; }
                    capture = lane;
                    captureDialog = new AlertDialog.Builder(activity).setTitle(LANES[lane])
                        .setMessage("割り当てるキー・ボタンを押してください。軸は中央に戻してから倒してください。")
                        .setNegativeButton("キャンセル", (c, which) -> capture = -1)
                        .setOnCancelListener(c -> capture = -1).create();
                    // Dialog がキーを受け取っても Activity と同じ経路で処理する。
                    captureDialog.setOnKeyListener((d2, code, event) -> key(event));
                    captureDialog.show();
                    captureDialog.getWindow().getDecorView().setOnGenericMotionListener((view, event) -> motion(event));
                }).show()).setNegativeButton("閉じる", null).show();
    }

    private void save() {
        SharedPreferences.Editor edit = prefs.edit();
        for (int i = 0; i < 4; i++) { edit.putStringSet("lane" + i, new HashSet<>(bindings[i])); }
        edit.apply();
    }

    private boolean press(int code, boolean down) {
        if (!down) { held.remove(code); return assigned(code); }
        if (!held.add(code)) { return capture >= 0 || assigned(code); }
        if (capture >= 0) {
            String id = Integer.toString(code);
            for (Set<String> lane : bindings) { lane.remove(id); }
            bindings[capture].add(id);
            capture = -1;
            save();
            if (captureDialog != null) { captureDialog.dismiss(); captureDialog = null; }
            return true;
        }
        int mask = 0;
        for (int i = 0; i < 4; i++) { if (bindings[i].contains(Integer.toString(code))) { mask |= 1 << i; } }
        final int bits = mask;
        if (bits != 0) {
            hits.getAndUpdate(old -> {
                int next = old;
                for (int lane = 0; lane < 4; lane++) {
                    int shift = lane * 4;
                    if ((bits & (1 << lane)) != 0 && ((next >> shift) & 15) < 15) { next += 1 << shift; }
                }
                return next;
            });
        }
        return mask != 0;
    }

    private boolean assigned(int code) {
        for (Set<String> lane : bindings) { if (lane.contains(Integer.toString(code))) { return true; } }
        return false;
    }

    boolean key(KeyEvent event) {
        int code = event.getKeyCode();
        if (code == KeyEvent.KEYCODE_BACK || code == KeyEvent.KEYCODE_VOLUME_DOWN || code == KeyEvent.KEYCODE_VOLUME_UP) { return false; }
        if (event.getAction() == KeyEvent.ACTION_DOWN || event.getAction() == KeyEvent.ACTION_UP) {
            return press(code, event.getAction() == KeyEvent.ACTION_DOWN);
        }
        return false;
    }

    boolean motion(MotionEvent event) {
        if ((event.getSource() & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK) { return false; }
        for (int axis : AXES) {
            float value = event.getAxisValue(axis);
            int negative = 1000 + axis * 2, positive = negative + 1;
            press(negative, value < (held.contains(negative) ? -0.35f : -0.6f));
            press(positive, value > (held.contains(positive) ? 0.35f : 0.6f));
        }
        return true;
    }

    int poll() { return hits.getAndSet(0); }
    void clear() { held.clear(); hits.set(0); }
}
