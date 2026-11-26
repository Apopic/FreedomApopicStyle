package com.example.freedomapopicstyle;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.database.Cursor;
import android.widget.Toast;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.concurrent.ExecutorService;

/** SAF の content URI はネイティブのファイル API では開けないため、必要なら作業フォルダへコピーする。 */
final class EditorFilePicker {
    static final int REQUEST = 4102;
    private final Activity activity;
    private final ExecutorService executor;
    private volatile String result = ""; // null = 選択中、空文字 = キャンセル
    private File directory;

    EditorFilePicker(Activity activity, ExecutorService executor) { this.activity = activity; this.executor = executor; }
    String poll() { return result; }

    void start(String mime, String destination) {
        result = null;
        activity.runOnUiThread(() -> {
            directory = new File(destination);
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT).setType(mime)
                .addCategory(Intent.CATEGORY_OPENABLE).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            try { activity.startActivityForResult(intent, REQUEST); }
            catch (RuntimeException e) { result = ""; Toast.makeText(activity, "ファイル選択アプリを開けません", Toast.LENGTH_LONG).show(); }
        });
    }

    void finish(int code, Intent data) {
        if (code != Activity.RESULT_OK || data == null || data.getData() == null) { result = ""; return; }
        Uri uri = data.getData();
        executor.execute(() -> {
            File partial = null;
            try {
                // 共有ストレージ内のファイルは元の場所を使い、TJA の相対音源パスを保つ。
                if ("com.android.externalstorage.documents".equals(uri.getAuthority()) && DocumentsContract.isDocumentUri(activity, uri)) {
                    String[] id = DocumentsContract.getDocumentId(uri).split(":", 2);
                    if (id.length == 2 && "primary".equalsIgnoreCase(id[0])) {
                        File root = android.os.Environment.getExternalStorageDirectory().getCanonicalFile();
                        File source = new File(root, id[1]).getCanonicalFile();
                        if (source.getPath().startsWith(root.getPath() + File.separator) && source.isFile() && source.canRead()) {
                            result = source.getPath(); return;
                        }
                    }
                }
                String name = "selected";
                try (Cursor cursor = activity.getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                    if (cursor != null && cursor.moveToFirst() && cursor.getString(0) != null) { name = cursor.getString(0); }
                }
                name = new File(name.replace('\\', '/')).getName().replaceAll("[\\/:*?\"<>|]", "_");
                if (name.isEmpty() || name.equals(".") || name.equals("..")) { name = "selected"; }
                if (!directory.isDirectory() && !directory.mkdirs()) { throw new java.io.IOException("保存先を作れません"); }
                File target = new File(directory, name);
                int dot = name.lastIndexOf('.');
                String base = dot > 0 ? name.substring(0, dot) : name, ext = dot > 0 ? name.substring(dot) : "";
                for (int i = 1; target.exists(); i++) { target = new File(directory, base + "_" + i + ext); }
                partial = File.createTempFile("pick-", ".part", directory);
                try (InputStream in = activity.getContentResolver().openInputStream(uri); FileOutputStream out = new FileOutputStream(partial)) {
                    if (in == null) { throw new java.io.IOException("読み込めません"); }
                    byte[] bytes = new byte[64 * 1024];
                    int size;
                    while ((size = in.read(bytes)) != -1) { out.write(bytes, 0, size); }
                }
                if (!partial.renameTo(target)) { throw new java.io.IOException("保存できません"); }
                result = target.getPath();
            } catch (Exception e) {
                result = "";
                activity.runOnUiThread(() -> Toast.makeText(activity, "ファイルを取得できません: " + e.getMessage(), Toast.LENGTH_LONG).show());
            } finally {
                if (partial != null && partial.exists()) { partial.delete(); }
            }
        });
    }
}
