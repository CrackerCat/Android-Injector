package cn.hluwa.injector;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

public class MainActivity extends Activity {


    static {
        Log.d("jnitest", "test: ");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        String outDir = getFilesDir().getAbsolutePath();
        try {
            writeAsset(outDir + "/injector32", "armeabi-v7a/injector");
            writeAsset(outDir + "/injector64", "arm64-v8a/injector");
            writeAsset(outDir + "/libshell32.so", "armeabi-v7a/libnative-lib.so");
            writeAsset(outDir + "/libshell64.so", "arm64-v8a/libnative-lib.so");
            String cmd = String.format("chmod 755 %s\n chmod 755 %s", outDir + "/injector32", outDir + "/injector64");
            exec(cmd);
        } catch (IOException e) {
            e.printStackTrace();
        }

        if (FindProcess("zygote")) {
            String cmd = "";
            cmd += outDir + "/injector32 " + outDir + "/libshell32.so &\n";
            exec(cmd);
            Log.d("inject", cmd);
        }
        if (FindProcess("zygote64")) {
            String cmd = "";
            cmd += outDir + "/injector64 " + outDir + "/libshell64.so &\n";
            exec(cmd);
            Log.d("inject", cmd);
        }
//        getAssets().open()

    }

    public boolean FindProcess(String name) {
        String[] ps = exec("ps | grep " + name).split("\n");
        for (String line : ps) {
            if (line.endsWith(name)) {
                return true;
            }
        }
        return false;
    }

    public void writeAsset(String outfile, String assetfile) throws IOException {
        File file = new File(outfile);
        file.mkdirs();
        if (file.exists()) {
            file.delete();
        }
        InputStream inputStream = getAssets().open(assetfile);
        byte data[] = new byte[inputStream.available()];
        inputStream.read(data, 0, data.length);
        OutputStream outputStream = new FileOutputStream(file);
        outputStream.write(data);
        inputStream.close();
        outputStream.close();
        Log.d("inject", "writeAsset: " + assetfile);
    }

    public static String exec(String cmd) {
        Process process = null;
        DataOutputStream os = null;
        BufferedReader is = null;
        String result = "";
        try {
            process = Runtime.getRuntime().exec("su"); //切换到root帐号
            os = new DataOutputStream(process.getOutputStream());
            is = new BufferedReader(new InputStreamReader(process.getInputStream()));
            os.writeBytes(cmd + "\n");
            os.writeBytes("exit\n");
            os.flush();
//            process.waitFor();
            String line;
            while ((line = is.readLine()) != null) {
                result = result + line + "\n";
            }
        } catch (Exception e) {
            return "";
        } finally {
            try {
                if (os != null) {
                    os.close();
                }
                process.destroy();
            } catch (Exception e) {
            }
        }
        return result;
    }
}
