package com.sty.ne.video.decodesync;

import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;

import java.io.File;

import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    private static final String FILE_DIR = Environment.getExternalStorageDirectory() + File.separator
            + "sty" + File.separator; //  /storage/emulated/0/sty/
    private SurfaceView surfaceView;
    private SeekBar seekBar;
    private Button btnOpen;

    private NePlayer nePlayer;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("neplayer");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        initView();
//        Log.e("sty", "file dir: " + FILE_DIR);
    }

    private void initView() {
        surfaceView = findViewById(R.id.surface_view);
        seekBar = findViewById(R.id.seek_bar);
        btnOpen = findViewById(R.id.btn_open);

        nePlayer = new NePlayer();
        nePlayer.setSurfaceView(surfaceView);

        btnOpen.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                open();
            }
        });
    }

    private void open() {
//        File file = new File(FILE_DIR, "input.mp4");
        File file = new File(Environment.getExternalStorageDirectory(), "input.mp4");
        nePlayer.start(file.getAbsolutePath());
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
}
