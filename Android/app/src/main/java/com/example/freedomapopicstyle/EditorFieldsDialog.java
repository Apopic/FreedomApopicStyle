package com.example.freedomapopicstyle;

import android.app.Activity;
import android.app.AlertDialog;
import android.text.InputType;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import org.json.JSONArray;
import org.json.JSONObject;
import java.util.ArrayList;
import java.util.List;

/** 値を同時に見比べながら変更できる、命令・ギミック・グラデーション共通の入力画面。 */
final class EditorFieldsDialog {
    private final Activity activity;
    private volatile String result = "";
    EditorFieldsDialog(Activity activity) { this.activity = activity; }
    String poll() { return result; }
    void start(String title, String schema) {
        result = null;
        activity.runOnUiThread(() -> {
            try {
                JSONArray fields = new JSONArray(schema);
                LinearLayout content = new LinearLayout(activity);
                content.setOrientation(LinearLayout.VERTICAL);
                int pad = (int)(16 * activity.getResources().getDisplayMetrics().density);
                content.setPadding(pad, pad, pad, pad);
                List<View> controls = new ArrayList<>();
                for (int i = 0; i < fields.length(); i++) {
                    JSONObject field = fields.getJSONObject(i);
                    TextView label = new TextView(activity); label.setText(field.getString("label")); content.addView(label);
                    String initial = field.getString("value");
                    JSONArray choices = field.getJSONArray("choices");
                    if (choices.length() > 0) {
                        List<String> items = new ArrayList<>();
                        for (int c = 0; c < choices.length(); c++) { items.add(choices.getString(c)); }
                        if (!items.contains(initial)) { items.add(initial); }
                        Spinner spinner = new Spinner(activity);
                        spinner.setAdapter(new ArrayAdapter<>(activity, android.R.layout.simple_spinner_dropdown_item, items));
                        spinner.setSelection(items.indexOf(initial)); content.addView(spinner); controls.add(spinner);
                    } else {
                        EditText input = new EditText(activity);
                        input.setFilters(new android.text.InputFilter[] { new android.text.InputFilter.LengthFilter(1024) });
                        input.setSingleLine(true);
                        input.setInputType(initial.matches("[-+]?[0-9]*\\.?[0-9]+") ?
                            InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_SIGNED | InputType.TYPE_NUMBER_FLAG_DECIMAL :
                            InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
                        input.setText(initial); input.setSelectAllOnFocus(true);
                        content.addView(input); controls.add(input);
                    }
                }
                ScrollView scroll = new ScrollView(activity); scroll.addView(content);
                AlertDialog dialog = new AlertDialog.Builder(activity).setTitle(title).setView(scroll)
                    .setPositiveButton("適用", (d, which) -> {
                        JSONArray values = new JSONArray();
                        for (View view : controls) { values.put(view instanceof EditText ? ((EditText)view).getText().toString() : ((Spinner)view).getSelectedItem().toString()); }
                        result = values.toString();
                    }).setNegativeButton("キャンセル", (d, which) -> result = "")
                    .setOnCancelListener(d -> result = "").create();
                dialog.show();
            } catch (Exception e) { result = ""; }
        });
    }
}
