package org.libsdl.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;

/**
 * Game assets live in /sdcard/kys_promise/ (user-editable). Android 11+ needs
 * "All files access" (MANAGE_EXTERNAL_STORAGE) to read that path via native I/O.
 */
public final class KysStoragePermission {
    private static final int REQ_READ_STORAGE = 9001;
    static boolean sWaitingForAccess = false;

    private KysStoragePermission() {}

    public static boolean hasAccess(Activity activity) {
        if (Build.VERSION.SDK_INT >= 30) {
            return Environment.isExternalStorageManager();
        }
        if (Build.VERSION.SDK_INT >= 23) {
            return activity.checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE)
                    == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    /** @return true if native startup may proceed */
    public static boolean ensure(Activity activity) {
        if (hasAccess(activity)) {
            sWaitingForAccess = false;
            return true;
        }
        sWaitingForAccess = true;
        showDialog(activity);
        return false;
    }

    public static void showDialog(Activity activity) {
        AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setTitle("需要存储权限");
        builder.setMessage(
                "游戏资源位于：\n/sdcard/kys_promise/\n\n"
                + "请在下一步授予「所有文件访问」权限，否则无法读取资源。");
        builder.setCancelable(false);
        builder.setPositiveButton("去授权", (d, w) -> openSettings(activity));
        builder.setNegativeButton("退出", (d, w) -> activity.finish());
        builder.create().show();
    }

    public static void openSettings(Activity activity) {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + activity.getPackageName()));
                activity.startActivity(intent);
            } catch (Exception e) {
                Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                activity.startActivity(intent);
            }
        } else if (Build.VERSION.SDK_INT >= 23) {
            activity.requestPermissions(
                    new String[]{
                            android.Manifest.permission.READ_EXTERNAL_STORAGE,
                            android.Manifest.permission.WRITE_EXTERNAL_STORAGE
                    },
                    REQ_READ_STORAGE);
        }
    }

    public static boolean onRequestPermissionsResult(Activity activity, int requestCode, int[] grantResults) {
        if (requestCode != REQ_READ_STORAGE) {
            return false;
        }
        if (hasAccess(activity)) {
            sWaitingForAccess = false;
            activity.recreate();
        } else {
            showDialog(activity);
        }
        return true;
    }

    public static void onResume(Activity activity) {
        if (sWaitingForAccess && hasAccess(activity)) {
            sWaitingForAccess = false;
            activity.recreate();
        }
    }
}
