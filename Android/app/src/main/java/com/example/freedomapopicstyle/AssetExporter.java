package com.example.freedomapopicstyle;

import android.content.Context;
import android.content.res.AssetManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
public class AssetExporter {

    public static void copyAssetsRecursively(Context context, String srcAssetPath, File destDir) throws IOException {
        AssetManager assetManager = context.getAssets();
        String[] assets = assetManager.list(srcAssetPath);

        if (assets == null || assets.length == 0) {
            copyFile(assetManager, srcAssetPath, destDir);
        } else {
            if (!destDir.exists()) {
                destDir.mkdirs();
            }
            for (String asset : assets) {
                String subSrcPath = srcAssetPath.isEmpty() ? asset : srcAssetPath + "/" + asset;
                File subDestDir = new File(destDir, asset);

                copyAssetsRecursively(context, subSrcPath, subDestDir);
            }
        }
    }

    private static void copyFile(AssetManager assetManager, String srcAssetPath, File destFile) throws IOException {
        File parent = destFile.getParentFile();
        if (parent != null && !parent.exists()) {
            parent.mkdirs();
        }

        try (InputStream in = assetManager.open(srcAssetPath);
             OutputStream out = new FileOutputStream(destFile)) {

            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
        }
    }
}
