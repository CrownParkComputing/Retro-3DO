package com.crownparkcomputing.retro3do;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Build;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;


/**
 * The Android entry point.
 *
 * Everything the emulator does lives in the native library; this class exists so
 * that Android-specific concerns — storage access, lifecycle, intents — have
 * somewhere to go that is not SDL's own activity. It is deliberately thin, and
 * should stay that way: logic that ends up here has to be written again for iOS.
 *
 * The methods below are called from C++ by name (see platform/android_storage.cpp).
 * They are static so the native side does not need an object reference, with a
 * single instance parked at creation — SDL only ever creates one activity.
 */
public class Retro3DOActivity extends SDLActivity {

    private static final String TAG = "Retro3DO";
    private static final float MAX_PRESENTATION_HZ = 120.0f;
    private static final int REQUEST_GPU_DRIVER = 7302;
    private static final long MAX_GPU_PACKAGE_BYTES = 512L * 1024L * 1024L;
    private static Retro3DOActivity instance;
    private DocumentAccess documents;
    // Consumed once by the native setup wizard after the asynchronous system
    // picker returns. Keeping the actual URI avoids guessing which persisted
    // grant is new when the user selects a folder that was already granted.
    private volatile String pickedFolder;
    private volatile String gpuDriverImportResult;

    @Override
    protected String[] getLibraries() {
        // Loaded in order. SDL first, because our library links against it.
        return new String[] {
            "SDL3",
            "main"
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        instance = this;
        documents = new DocumentAccess(this);
        super.onCreate(savedInstanceState);
        requestFastestDisplayMode();
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Android may restore its default mode while the activity is paused.
        // Reassert the request when returning to the foreground.
        requestFastestDisplayMode();
    }

    /**
     * Ask Android for the fastest native panel mode no higher than 120 Hz.
     *
     * The emulated 3DO remains at its real 50/60 Hz field rate. A faster panel
     * makes presentation and touch/UI response smoother; it must not make the
     * machine or its audio run fast. SDL's renderer stays vsynced to whichever
     * mode Android grants, so this also retains tear-free presentation.
     */
    @SuppressWarnings("deprecation") // Required below API 30; minSdk is 24.
    private void requestFastestDisplayMode() {
        Display display = getWindowManager().getDefaultDisplay();
        Display.Mode current = display.getMode();
        Display.Mode best = current;

        for (Display.Mode candidate : display.getSupportedModes()) {
            // Do not trade resolution for refresh rate. Rotation does not
            // change these physical mode dimensions.
            if (candidate.getPhysicalWidth() != current.getPhysicalWidth() ||
                candidate.getPhysicalHeight() != current.getPhysicalHeight()) {
                continue;
            }
            final float hz = candidate.getRefreshRate();
            if (hz <= MAX_PRESENTATION_HZ + 0.01f &&
                hz > best.getRefreshRate() + 0.01f) {
                best = candidate;
            }
        }

        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.preferredDisplayModeId = best.getModeId();
        attributes.preferredRefreshRate = best.getRefreshRate();
        getWindow().setAttributes(attributes);
        Log.i(TAG, "Requested display mode " + best.getPhysicalWidth() + "x" +
                   best.getPhysicalHeight() + " @ " + best.getRefreshRate() + " Hz");
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == DocumentAccess.REQUEST_PICK_FOLDER) {
            if (resultCode == RESULT_OK && data != null) {
                Uri tree = data.getData();
                documents.onFolderGranted(tree);
                pickedFolder = tree != null ? tree.toString() : null;
            } else {
                pickedFolder = "";
            }
            // A cancelled pick is not an error; the browser simply shows no new
            // folder and the user can try again.
            return;
        }
        if (requestCode == REQUEST_GPU_DRIVER) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) {
                final Uri packageUri = data.getData();
                gpuDriverImportResult = null;
                new Thread(new Runnable() {
                    @Override public void run() { importGpuDriver(packageUri); }
                }, "Retro3DO-driver-import").start();
            }
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    // --- called from native ------------------------------------------------

    public static void nativePickFolder() {
        final Retro3DOActivity self = instance;
        if (self == null) return;
        self.pickedFolder = null;
        // The picker must be started from the UI thread; the emulator's thread
        // is what calls in here.
        self.runOnUiThread(new Runnable() {
            @Override public void run() { self.documents.pickFolder(); }
        });
    }

    public static String nativeGrantedRoots() {
        if (instance == null) return "";
        String[] roots = instance.documents.grantedRoots();
        StringBuilder out = new StringBuilder();
        for (String root : roots) {
            out.append(instance.documents.displayNameOf(root)).append('|')
               .append(root).append('\n');
        }
        return out.toString();
    }

    public static String nativeConsumePickedFolder() {
        if (instance == null || instance.pickedFolder == null) return "";
        String uri = instance.pickedFolder;
        instance.pickedFolder = null;
        if (uri.isEmpty()) return "|"; // completed cancellation
        return instance.documents.displayNameOf(uri) + "|" + uri;
    }

    public static String nativeListFolder(String uri) {
        if (instance == null) return "";
        return instance.documents.listFolder(uri);
    }

    public static String nativeParentFolder(String uri) {
        if (instance == null) return "";
        return instance.documents.parentFolderOf(uri);
    }

    public static String nativeEnsureDirectory(String parent, String name) {
        if (instance == null) return "";
        try {
            return instance.documents.ensureDirectory(parent, name);
        } catch (Exception error) {
            Log.w(TAG, "Could not create " + name, error);
            return "";
        }
    }

    public static int nativeOpenWriteDocument(String folder, String name) {
        return instance == null ? -1 : instance.documents.openWriteDocument(folder, name);
    }

    public static int nativeOpenDocument(String uri) {
        if (instance == null) return -1;
        return instance.documents.openDocument(uri);
    }

    public static void nativeForgetRoot(String uri) {
        if (instance == null) return;
        instance.documents.forgetRoot(uri);
    }

    public static void nativePickGpuDriver() {
        final Retro3DOActivity self = instance;
        if (self == null) return;
        self.runOnUiThread(new Runnable() {
            @Override public void run() {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("application/zip");
                intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
                    "application/zip", "application/x-zip-compressed",
                    "application/octet-stream"
                });
                self.startActivityForResult(intent, REQUEST_GPU_DRIVER);
            }
        });
    }

    public static String nativeConsumeGpuDriverImport() {
        if (instance == null || instance.gpuDriverImportResult == null) return "";
        String result = instance.gpuDriverImportResult;
        instance.gpuDriverImportResult = null;
        return result;
    }

    public static String nativeLibraryDirectory() {
        if (instance == null) return "";
        return instance.getApplicationInfo().nativeLibraryDir;
    }

    private static String safeResultField(String text) {
        if (text == null) return "";
        return text.replace('|', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private static void deleteTree(File file) {
        if (file == null || !file.exists()) return;
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) deleteTree(child);
        }
        // Only ever called for the newly-created private import directory.
        if (!file.delete()) Log.w(TAG, "Could not remove failed driver import " + file);
    }

    private static boolean isArm64Elf(File file) {
        byte[] header = new byte[20];
        try (FileInputStream input = new FileInputStream(file)) {
            if (input.read(header) != header.length) return false;
        } catch (Exception error) {
            return false;
        }
        // ELF64, little endian, EM_AARCH64 (183 / 0xB7).
        return (header[0] & 0xff) == 0x7f && header[1] == 'E' &&
               header[2] == 'L' && header[3] == 'F' && header[4] == 2 &&
               header[5] == 1 && (header[18] & 0xff) == 0xb7 && header[19] == 0;
    }

    private void importGpuDriver(Uri packageUri) {
        if (Build.VERSION.SDK_INT < 28) {
            gpuDriverImportResult = "ERROR|Custom drivers require Android 9 or newer";
            return;
        }

        File destination = new File(new File(getFilesDir(), "gpu-drivers"),
                                    "driver-" + System.currentTimeMillis());
        try {
            if (!destination.mkdirs()) throw new Exception("Cannot create private driver folder");

            JSONObject metadata = null;
            ArrayList<String> libraries = new ArrayList<>();
            long totalBytes = 0;
            int entryCount = 0;
            byte[] buffer = new byte[64 * 1024];

            try (InputStream source = getContentResolver().openInputStream(packageUri);
                 ZipInputStream zip = new ZipInputStream(source)) {
                if (source == null) throw new Exception("Cannot open selected package");
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    if (++entryCount > 128) throw new Exception("Driver package has too many files");
                    if (entry.isDirectory()) continue;

                    String name = entry.getName();
                    if (name == null || name.isEmpty() || name.contains("/") ||
                        name.contains("\\") || name.contains("..")) {
                        throw new Exception("Driver package contains an unsafe path");
                    }
                    boolean isMetadata = name.equals("meta.json");
                    boolean isLibrary = name.endsWith(".so");
                    if (!isMetadata && !isLibrary) continue;

                    ByteArrayOutputStream metadataBytes =
                        isMetadata ? new ByteArrayOutputStream() : null;
                    File output = isLibrary ? new File(destination, name) : null;
                    try (FileOutputStream libraryOutput =
                             isLibrary ? new FileOutputStream(output) : null) {
                        long fileBytes = 0;
                        int count;
                        while ((count = zip.read(buffer)) > 0) {
                            fileBytes += count;
                            totalBytes += count;
                            if (fileBytes > 256L * 1024L * 1024L ||
                                totalBytes > MAX_GPU_PACKAGE_BYTES) {
                                throw new Exception("Driver package is too large");
                            }
                            if (isMetadata) {
                                if (fileBytes > 1024L * 1024L) {
                                    throw new Exception("Driver metadata is too large");
                                }
                                metadataBytes.write(buffer, 0, count);
                            } else {
                                libraryOutput.write(buffer, 0, count);
                            }
                        }
                    }

                    if (isMetadata) {
                        metadata = new JSONObject(
                            new String(metadataBytes.toByteArray(), StandardCharsets.UTF_8));
                    } else {
                        if (!isArm64Elf(output)) {
                            throw new Exception(name + " is not an ARM64 Android library");
                        }
                        output.setReadable(true, true);
                        output.setExecutable(true, true);
                        libraries.add(name);
                    }
                }
            }

            if (metadata == null) throw new Exception("Package has no meta.json");
            if (metadata.optInt("schemaVersion", 0) != 1) {
                throw new Exception("Unsupported driver package schema");
            }
            if (metadata.optInt("minApi", 0) > Build.VERSION.SDK_INT) {
                throw new Exception("Driver requires a newer Android version");
            }

            String libraryName = metadata.optString("libraryName", "");
            if (libraryName.isEmpty() || !libraries.contains(libraryName)) {
                libraryName = "";
                for (String candidate : libraries) {
                    if (candidate.equals("libvulkan_freedreno.so") ||
                        candidate.startsWith("vulkan.") ||
                        candidate.toLowerCase().contains("vulkan")) {
                        libraryName = candidate;
                        break;
                    }
                }
            }
            if (libraryName.isEmpty()) throw new Exception("Package has no Vulkan driver library");

            String name = metadata.optString("name", "Custom Adreno driver");
            String version = metadata.optString("driverVersion", "");
            if (!version.isEmpty()) name += " " + version;
            gpuDriverImportResult = "OK|" + safeResultField(name) + "|" +
                                    destination.getAbsolutePath() + File.separator + "|" +
                                    libraryName;
            Log.i(TAG, "Imported GPU driver " + name + " / " + libraryName);
        } catch (Exception error) {
            deleteTree(destination);
            gpuDriverImportResult = "ERROR|" + safeResultField(error.getMessage());
            Log.e(TAG, "GPU driver import failed", error);
        }
    }

}
