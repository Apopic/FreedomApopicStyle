package com.example.freedomapopicstyle;

import android.app.AlertDialog;
import android.app.NativeActivity;

import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;

import android.annotation.TargetApi;

import android.database.Cursor;
import android.provider.OpenableColumns;

import com.yausername.ffmpeg.FFmpeg;
import com.yausername.youtubedl_android.YoutubeDL;
import com.yausername.youtubedl_android.YoutubeDLRequest;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class MainActivity extends NativeActivity {
    private ExternalInput externalInput;
    private EditorFilePicker editorFilePicker;
    private EditorFieldsDialog editorFieldsDialog;
    public void StartEditorFields(String title, String schema) { editorFieldsDialog.start(title, schema); }
    public String PollEditorFields() { return editorFieldsDialog == null ? "" : editorFieldsDialog.poll(); }
    public void StartEditorFilePicker(String mime, String destination) { editorFilePicker.start(mime, destination); }
    public String PollEditorFilePicker() { return editorFilePicker == null ? "" : editorFilePicker.poll(); }
    @Override protected void onActivityResult(int request, int code, Intent data) {
        super.onActivityResult(request, code, data);
        if (request == EditorFilePicker.REQUEST) { editorFilePicker.finish(code, data); }
    }

    public void ConfigureExternalInput() { runOnUiThread(() -> externalInput.configure()); }
    public int PollExternalInput() { return externalInput == null ? 0 : externalInput.poll(); }

    @Override public boolean dispatchKeyEvent(android.view.KeyEvent event) {
        return (externalInput != null && externalInput.key(event)) || super.dispatchKeyEvent(event);
    }
    @Override public boolean dispatchGenericMotionEvent(android.view.MotionEvent event) {
        return (externalInput != null && externalInput.motion(event)) || super.dispatchGenericMotionEvent(event);
    }
    @Override public void onWindowFocusChanged(boolean focus) {
        super.onWindowFocusChanged(focus);
        if (!focus && externalInput != null) { externalInput.clear(); }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        externalInput = new ExternalInput(this);
        editorFilePicker = new EditorFilePicker(this, executorService);
        editorFieldsDialog = new EditorFieldsDialog(this);
        setContentView(R.layout.activity_main);
        // ファイルへのアクセスが許可されてからスキンを書き出す (許可の前に書き出すと失敗して、読み込み中のまま止まる)
        if (hasStorageAccess()) {
            startAssetCopyProcess();
        }
        else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            requestAllFilesAccessPermission();
        }
        else {
            requestPermissions(new String[] { android.Manifest.permission.READ_EXTERNAL_STORAGE, android.Manifest.permission.WRITE_EXTERNAL_STORAGE }, 1);
        }

        handleImportIntent(getIntent());
    }

    private boolean hasStorageAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) { return Environment.isExternalStorageManager(); }
        return checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE) == android.content.pm.PackageManager.PERMISSION_GRANTED;
    }

    // 設定画面で「すべてのファイルへのアクセス」を許可して戻ってきた
    @Override
    protected void onResume() {
        super.onResume();
        if (mAssetCopyState == 0 && !mAssetCopying && hasStorageAccess()) { startAssetCopyProcess(); }
    }

    // Android 9・10 の許可のダイアログ
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (mAssetCopyState == 0 && !mAssetCopying && hasStorageAccess()) { startAssetCopyProcess(); }
        else if (!hasStorageAccess()) { ShowToast("譜面とスキンを置くには、ファイルへのアクセスの許可が必要です"); }
    }

    // アプリを開いている時に、ファイル管理アプリから zip を開いた・共有した
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleImportIntent(intent);
    }

    // zip のインポートが終わるたびに増やす (ネイティブ側が見て、譜面の一覧を作り直す)
    volatile int ImportSerial = 0;

    private void handleImportIntent(Intent intent) {
        if (intent == null) { return; }
        final String action = intent.getAction();
        final List<Uri> uris = new ArrayList<>();
        if (Intent.ACTION_VIEW.equals(action) && intent.getData() != null) {
            uris.add(intent.getData());
        }
        else if (Intent.ACTION_SEND.equals(action)) {
            Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (uri == null) { uri = intent.getData(); }
            if (uri != null) { uris.add(uri); }
        }
        else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
            ArrayList<Uri> list = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
            if (list != null) { uris.addAll(list); }
        }
        if (uris.isEmpty()) { return; }
        // 解凍は時間がかかるので別スレッドで行う
        executorService.execute(() -> {
            int total = 0;
            for (Uri uri : uris) { total += unzipFromUri(uri); }
            final int charts = total;
            ImportSerial++;
            runOnUiThread(() -> Toast.makeText(this, charts > 0 ? "譜面をインポートしました (" + charts + " 曲)" : "zip に譜面 (.tja / .osu) がありませんでした", Toast.LENGTH_LONG).show());
        });
    }

    private String displayName(Uri uri) {
        try (Cursor c = getContentResolver().query(uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                String name = c.getString(0);
                if (name != null) { return name; }
            }
        } catch (Exception ignored) {
        }
        String last = uri.getLastPathSegment();
        return last != null ? last : "import.zip";
    }

    // zip のエントリの一覧 (文字コードを確かめる。日本語の zip は Shift_JIS のことが多い)
    private List<String> listEntries(Uri uri, Charset charset) throws IOException {
        List<String> names = new ArrayList<>();
        try (InputStream in = getContentResolver().openInputStream(uri); ZipInputStream zis = new ZipInputStream(in, charset)) {
            ZipEntry e;
            while ((e = zis.getNextEntry()) != null) { names.add(e.getName()); }
        }
        return names;
    }

    // zip を Songs フォルダへ解凍する。戻り値: 中の .tja の数
    //   ・zip の中身が 1 つのフォルダにまとまっていなければ、zip の名前のフォルダを作ってその中に入れる
    //   ・"../" などで Songs の外へ書き込むエントリ (Zip Slip) は書き込まない
    private int unzipFromUri(Uri zipUri) {
        File songs = new File("/storage/emulated/0/Documents/FAS/Songs");
        if (!songs.exists()) { songs.mkdirs(); }
        try {
            Charset charset = Charset.forName("UTF-8");
            List<String> names;
            try {
                names = listEntries(zipUri, charset);
            } catch (IllegalArgumentException | java.util.zip.ZipException e) {
                charset = Charset.forName("MS932");
                names = listEntries(zipUri, charset);
            }
            // 最上位のフォルダが 1 つだけか
            String top = null;
            boolean single = !names.isEmpty();
            int charts = 0;
            for (String n : names) {
                if (n.toLowerCase(java.util.Locale.ROOT).endsWith(".tja") || n.toLowerCase(java.util.Locale.ROOT).endsWith(".osu")) { charts++; }
                int slash = n.indexOf('/');
                String head = slash >= 0 ? n.substring(0, slash) : null;
                if (head == null || (top != null && !top.equals(head))) { single = false; }
                if (top == null) { top = head; }
            }
            File target = songs;
            if (!single) {
                String zipname = displayName(zipUri);
                String extension = zipname.toLowerCase(java.util.Locale.ROOT);
                if (extension.endsWith(".zip") || extension.endsWith(".osz")) { zipname = zipname.substring(0, zipname.length() - 4); }
                zipname = zipname.replaceAll("[\\\\/:*?\"<>|]", "_");
                target = new File(songs, zipname.isEmpty() ? "import" : zipname);
            }
            final String root = target.getCanonicalPath() + File.separator;
            byte[] buffer = new byte[64 * 1024];
            try (InputStream in = getContentResolver().openInputStream(zipUri); ZipInputStream zis = new ZipInputStream(in, charset)) {
                ZipEntry entry;
                while ((entry = zis.getNextEntry()) != null) {
                    File out = new File(target, entry.getName());
                    if (!out.getCanonicalPath().startsWith(root)) { continue; }     // Zip Slip
                    if (entry.isDirectory()) { out.mkdirs(); continue; }
                    File parent = out.getParentFile();
                    if (parent != null && !parent.exists()) { parent.mkdirs(); }
                    try (FileOutputStream fos = new FileOutputStream(out)) {
                        int len;
                        while ((len = zis.read(buffer)) > 0) { fos.write(buffer, 0, len); }
                    }
                    zis.closeEntry();
                }
            }
            return charts;
        } catch (Exception e) {
            e.printStackTrace();
            final String msg = e.getMessage();
            runOnUiThread(() -> Toast.makeText(this, "解凍に失敗しました: " + msg, Toast.LENGTH_LONG).show());
            return 0;
        }
    }

    // ---- SONGLINK のダウンロード (youtubedl-android: Python・yt-dlp・ffmpeg をアプリに同梱) ----
    private boolean mYtdlReady = false;
    private boolean mYtdlUpdated = false;
    String DownloadError = "";

    // ネイティブ側から別スレッドで呼ぶ (終わるまで戻らない)。成功したら保存したファイルのパス、失敗したら空文字
    public synchronized String DownloadMedia(String url, String outDir, boolean audio) {
        DownloadError = "";
        try {
            if (!mYtdlReady) {
                YoutubeDL.getInstance().init(getApplication());
                FFmpeg.getInstance().init(getApplication());
                mYtdlReady = true;
            }
            if (!mYtdlUpdated) {
                // サイトの変更に追従するため、起動して最初のダウンロードの前に yt-dlp を更新する (失敗しても続ける)
                try { YoutubeDL.getInstance().updateYoutubeDL(getApplication(), YoutubeDL.UpdateChannel._STABLE); } catch (Exception ignored) { }
                mYtdlUpdated = true;
            }
            File dir = new File(outDir);
            dir.mkdirs();
            File[] old = dir.listFiles((d, name) -> name.startsWith("media."));
            if (old != null) { for (File f : old) { f.delete(); } }
            YoutubeDLRequest request = new YoutubeDLRequest(url);
            request.addOption("--no-playlist");
            request.addOption("-o", new File(dir, "media.%(ext)s").getAbsolutePath());
            if (audio) {
                request.addOption("-x");
                request.addOption("--audio-format", "vorbis");
            }
            YoutubeDL.getInstance().execute(request, (String) null, false, null);
            File[] files = dir.listFiles((d, name) -> name.startsWith("media.") && !name.endsWith(".part"));
            if (files == null || files.length == 0) { DownloadError = "ファイルができませんでした"; return ""; }
            return files[0].getAbsolutePath();
        } catch (Exception e) {
            DownloadError = String.valueOf(e.getMessage());
            return "";
        }
    }

    @TargetApi(Build.VERSION_CODES.R)
    private void requestAllFilesAccessPermission() {
        try {
            Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
            intent.setData(Uri.parse("package:" + getPackageName()));
            startActivity(intent);
        } catch (Exception e) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
            startActivity(intent);
        }
    }

    static {
        System.loadLibrary("native-lib");
    }

    private final ExecutorService executorService = Executors.newSingleThreadExecutor();

    volatile int mAssetCopyState;
    private volatile boolean mAssetCopying = false;

    private void startAssetCopyProcess() {
        mAssetCopying = true;
        File SongDir = new File("/storage/emulated/0/Documents/FAS/Songs");
        if (!SongDir.exists()) {
            SongDir.mkdirs();
        }

        File targetDir = new File("/storage/emulated/0/Documents/FAS/Skins/Default");

        if (targetDir.exists() && targetDir.list() != null && Objects.requireNonNull(targetDir.list()).length > 0) {
            mAssetCopyState = 1;
            mAssetCopying = false;
            ImportSerial++;     // 許可の前に作った譜面の一覧を作り直してもらう
            return;
        }

        executorService.execute(() -> {
            try {
                AssetExporter.copyAssetsRecursively(MainActivity.this, "Default", targetDir);

                runOnUiThread(() -> {
                    Toast.makeText(MainActivity.this, "アセットの読み込みが完了しました", Toast.LENGTH_SHORT).show();
                    mAssetCopyState = 1;
                    mAssetCopying = false;
                    ImportSerial++;
                });
            } catch (IOException e) {
                e.printStackTrace();
                runOnUiThread(() -> {
                    Toast.makeText(MainActivity.this, "アセットのコピーに失敗しました", Toast.LENGTH_LONG).show();
                    mAssetCopying = false;
                });
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        executorService.shutdown();
    }

    public void ShowToast(String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(getApplicationContext(), text, Toast.LENGTH_SHORT).show();
            }
        });
    }

    EditText EditView;
    AlertDialog.Builder AlBuilder;

    volatile String InputString;

    volatile int InputEnd;

    public void StartInputStringDialog(String Theme, String initial) {
        MainActivity AppActivity = this;
        InputEnd = 0;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                EditView = new EditText(AppActivity);
                EditView.setText(initial);
                EditView.selectAll();
                AlBuilder = new AlertDialog.Builder(AppActivity);
                AlBuilder.setTitle(Theme);
                AlBuilder.setView(EditView);
                AlBuilder.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        InputString = EditView.getText().toString();
                        InputEnd = 1;
                        View decor = AppActivity.getWindow().getDecorView();
                        decor.setSystemUiVisibility(
                                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                                        View.SYSTEM_UI_FLAG_LOW_PROFILE |
                                        View.SYSTEM_UI_FLAG_FULLSCREEN |
                                        View.SYSTEM_UI_FLAG_IMMERSIVE |
                                        View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
                    }
                });
                // キャンセル・ボックスの外を押した・戻るキー: 空の文字列で終える (ゲーム側が入力の終わりを待ち続けないように)
                AlBuilder.setNegativeButton("キャンセル", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        InputString = "";
                        InputEnd = 2;
                    }
                });
                AlBuilder.setOnCancelListener(new DialogInterface.OnCancelListener() {
                    public void onCancel(DialogInterface dialog) {
                        InputString = "";
                        InputEnd = 2;
                    }
                });
                AlBuilder.show();
            }
        });
    }

    // はい / いいえ のダイアログ (はい: InputString = "1"、いいえ・キャンセル: "")
    public void StartYesNoDialog(String title, String message) {
        MainActivity AppActivity = this;
        InputEnd = 0;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                AlertDialog.Builder b = new AlertDialog.Builder(AppActivity);
                b.setTitle(title);
                b.setMessage(message);
                b.setPositiveButton("はい", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) { InputString = "1"; InputEnd = 1; }
                });
                b.setNegativeButton("いいえ", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) { InputString = ""; InputEnd = 1; }
                });
                b.setOnCancelListener(new DialogInterface.OnCancelListener() {
                    public void onCancel(DialogInterface dialog) { InputString = ""; InputEnd = 1; }
                });
                b.show();
            }
        });
    }
}
